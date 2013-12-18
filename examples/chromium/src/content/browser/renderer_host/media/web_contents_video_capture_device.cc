// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation notes: This needs to work on a variety of hardware
// configurations where the speed of the CPU and GPU greatly affect overall
// performance. Spanning several threads, the process of capturing has been
// split up into four conceptual stages:
//
//   1. Reserve Buffer: Before a frame can be captured, a slot in the consumer's
//      shared-memory IPC buffer is reserved. There are only a few of these;
//      when they run out, it indicates that the downstream consumer -- likely a
//      video encoder -- is the performance bottleneck, and that the rate of
//      frame capture should be throttled back.
//
//   2. Capture: A bitmap is snapshotted/copied from the RenderView's backing
//      store. This is initiated on the UI BrowserThread, and often occurs
//      asynchronously. Where supported, the GPU scales and color converts
//      frames to our desired size, and the readback happens directly into the
//      shared-memory buffer. But this is not always possible, particularly when
//      accelerated compositing is disabled.
//
//   3. Render (if needed): If the web contents cannot be captured directly into
//      our target size and color format, scaling and colorspace conversion must
//      be done on the CPU. A dedicated thread is used for this operation, to
//      avoid blocking the UI thread. The Render stage always reads from a
//      bitmap returned by Capture, and writes into the reserved slot in the
//      shared-memory buffer.
//
//   4. Deliver: The rendered video frame is returned to the consumer (which
//      implements the VideoCaptureDevice::EventHandler interface). Because
//      all paths have written the frame into the IPC buffer, this step should
//      never need to do an additional copy of the pixel data.
//
// In the best-performing case, the Render step is bypassed: Capture produces
// ready-to-Deliver frames. But when accelerated readback is not possible, the
// system is designed so that Capture and Render may run concurrently. A timing
// diagram helps illustrate this point (@30 FPS):
//
//    Time: 0ms                 33ms                 66ms                 99ms
// thread1: |-Capture-f1------v |-Capture-f2------v  |-Capture-f3----v    |-Capt
// thread2:                   |-Render-f1-----v   |-Render-f2-----v  |-Render-f3
//
// In the above example, both capturing and rendering *each* take almost the
// full 33 ms available between frames, yet we see that the required throughput
// is obtained.
//
// Turning on verbose logging will cause the effective frame rate to be logged
// at 5-second intervals.

#include "content/browser/renderer_host/media/web_contents_video_capture_device.h"

#include <algorithm>
#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "content/browser/renderer_host/media/web_contents_capture_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/port/browser/render_widget_host_view_frame_subscriber.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/bind_to_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/yuv_convert.h"
#include "media/video/capture/video_capture_types.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"

namespace content {

namespace {

const int kMinFrameWidth = 2;
const int kMinFrameHeight = 2;
const int kMaxFramesInFlight = 2;
const int kMaxSnapshotsInFlight = 1;

// This value controls how many redundant, timer-base captures occur when the
// content is static. Redundantly capturing the same frame allows iterative
// quality enhancement, and also allows the buffer to fill in "buffered mode".
//
// TODO(nick): Controlling this here is a hack and a layering violation, since
// it's a strategy specific to the WebRTC consumer, and probably just papers
// over some frame dropping and quality bugs. It should either be controlled at
// a higher level, or else redundant frame generation should be pushed down
// further into the WebRTC encoding stack.
const int kNumRedundantCapturesOfStaticContent = 200;

// TODO(nick): Remove this once frame subscription is supported on Aura and
// Linux.
#if (defined(OS_WIN) || defined(OS_MACOSX)) || defined(USE_AURA)
const bool kAcceleratedSubscriberIsSupported = true;
#else
const bool kAcceleratedSubscriberIsSupported = false;
#endif

typedef base::Callback<void(base::Time, bool)> DeliverFrameCallback;

// Returns the nearest even integer closer to zero.
template<typename IntType>
IntType MakeEven(IntType x) {
  return x & static_cast<IntType>(-2);
}

// Compute a letterbox region, aligned to even coordinates.
gfx::Rect ComputeYV12LetterboxRegion(const gfx::Size& frame_size,
                                     const gfx::Size& content_size) {

  gfx::Rect result = media::ComputeLetterboxRegion(gfx::Rect(frame_size),
                                                   content_size);

  result.set_x(MakeEven(result.x()));
  result.set_y(MakeEven(result.y()));
  result.set_width(std::max(kMinFrameWidth, MakeEven(result.width())));
  result.set_height(std::max(kMinFrameHeight, MakeEven(result.height())));

  return result;
}

// CaptureOracle is informed of every update to a particular web content view.
// This empowers it look into the future and decide whether a particular frame
// ought to be captured in order to achieve its target frame rate.
class CaptureOracle : public base::RefCountedThreadSafe<CaptureOracle> {
 public:
  enum Event {
    TIMER_POLL,
    COMPOSITOR_UPDATE,
    SOFTWARE_PAINT
  };

  CaptureOracle(media::VideoCaptureDevice::EventHandler* consumer,
                base::TimeDelta capture_period);

  // Record an event of type |event|, and decide whether the caller should do a
  // frame capture immediately. Decisions of the oracle are final: the caller
  // must do what it is told.
  //
  // If a capture should not occur, returns false. Otherwise returns true, and
  // |storage| and |callback| will be populated with a buffer into which a
  // capture should be done, and a callback to invoke once the frame is ready.
  bool ObserveEventAndDecideCapture(
      Event event,
      base::Time event_time,
      scoped_refptr<media::VideoFrame>* storage,
      DeliverFrameCallback* callback);

  base::TimeDelta capture_period() const { return capture_period_; }

  // Allow new captures to start occuring.
  void Start();

  // Stop new captures from happening (but doesn't forget the consumer).
  void Stop();

  // Signal an error to the consumer.
  void ReportError();

  // Permanently stop capturing. Immediately cease all activity on the
  // VCD::EventHandler.
  void InvalidateConsumer();

 private:
  friend class base::RefCountedThreadSafe<CaptureOracle>;
  virtual ~CaptureOracle() {}

  // Callback invoked upon completion of all captures.
  void DidCaptureFrame(const scoped_refptr<media::VideoFrame>& frame,
                       int frame_number,
                       base::Time timestamp,
                       bool success);

  // Time between frames.
  const base::TimeDelta capture_period_;

  // Protects everything below it.
  base::Lock lock_;

  // Recipient of our capture activity. Becomes null after it is invalidated.
  media::VideoCaptureDevice::EventHandler* consumer_;

  // Incremented every time a paint or update event occurs.
  int frame_number_;

  // Stores the frame number from the last delivered frame.
  int last_delivered_frame_number_;

  // Stores the timestamp of the last delivered frame.
  base::Time last_delivered_frame_timestamp_;

  // Whether capturing is currently allowed. Can toggle back and forth.
  bool is_started_;

  // Tracks present/paint history.
  SmoothEventSampler sampler_;
};

// FrameSubscriber is a proxy to the CaptureOracle that's compatible with
// RenderWidgetHostViewFrameSubscriber. We create one per event type.
class FrameSubscriber : public RenderWidgetHostViewFrameSubscriber {
 public:
  FrameSubscriber(CaptureOracle::Event event_type,
                  const scoped_refptr<CaptureOracle>& oracle)
      : event_type_(event_type),
        oracle_(oracle) {}

  virtual bool ShouldCaptureFrame(
      base::Time present_time,
      scoped_refptr<media::VideoFrame>* storage,
      RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback*
          deliver_frame_cb) OVERRIDE;

 private:
  const CaptureOracle::Event event_type_;
  scoped_refptr<CaptureOracle> oracle_;
};

// ContentCaptureSubscription is the relationship between a RenderWidgetHost
// whose content is updating, a subscriber that is deciding which of these
// updates to capture (and where to deliver them to), and a callback that
// knows how to do the capture and prepare the result for delivery.
//
// In practice, this means (a) installing a RenderWidgetHostFrameSubscriber in
// the RenderWidgetHostView, to process updates that occur via accelerated
// compositing, (b) installing itself as an observer of updates to the
// RenderWidgetHost's backing store, to hook updates that occur via software
// rendering, and (c) running a timer to possibly initiate non-event-driven
// captures that the subscriber might request.
//
// All of this happens on the UI thread, although the
// RenderWidgetHostViewFrameSubscriber we install may be dispatching updates
// autonomously on some other thread.
class ContentCaptureSubscription : public content::NotificationObserver {
 public:
  typedef base::Callback<void(const base::Time&,
                              const scoped_refptr<media::VideoFrame>&,
                              const DeliverFrameCallback&)> CaptureCallback;

  // Create a subscription. Whenever a manual capture is required, the
  // subscription will invoke |capture_callback| on the UI thread to do the
  // work.
  ContentCaptureSubscription(
      const RenderWidgetHost& source,
      const scoped_refptr<CaptureOracle>& oracle,
      const CaptureCallback& capture_callback);
  virtual ~ContentCaptureSubscription();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void OnTimer();

  const int render_process_id_;
  const int render_view_id_;

  FrameSubscriber paint_subscriber_;
  FrameSubscriber timer_subscriber_;
  content::NotificationRegistrar registrar_;
  CaptureCallback capture_callback_;
  base::Timer timer_;

  DISALLOW_COPY_AND_ASSIGN(ContentCaptureSubscription);
};

// Render the SkBitmap |input| into the given VideoFrame buffer |output|, then
// invoke |done_cb| to indicate success or failure. |input| is expected to be
// ARGB. |output| must be YV12 or I420. Colorspace conversion is always done.
// Scaling and letterboxing will be done as needed.
//
// This software implementation should be used only when GPU acceleration of
// these activities is not possible. This operation may be expensive (tens to
// hundreds of milliseconds), so the caller should ensure that it runs on a
// thread where such a pause would cause UI jank.
void RenderVideoFrame(const SkBitmap& input,
                      const scoped_refptr<media::VideoFrame>& output,
                      const base::Callback<void(bool)>& done_cb);

// Keeps track of the RenderView to be sourced, and executes copying of the
// backing store on the UI BrowserThread.
//
// TODO(nick): It would be nice to merge this with WebContentsTracker, but its
// implementation is currently asynchronous -- in our case, the "rvh changed"
// notification would get posted back to the UI thread and processed later, and
// this seems disadvantageous.
class CaptureMachine : public WebContentsObserver,
                       public base::SupportsWeakPtr<CaptureMachine> {
 public:
  virtual ~CaptureMachine();

  // Creates a CaptureMachine. Must be run on the UI BrowserThread. Returns
  // NULL if the indicated render view cannot be found.
  static scoped_ptr<CaptureMachine> Create(
      int render_process_id,
      int render_view_id,
      const scoped_refptr<base::SequencedTaskRunner>& render_task_runner,
      const scoped_refptr<CaptureOracle>& oracle);

  // Starts a copy from the backing store or the composited surface. Must be run
  // on the UI BrowserThread. |deliver_frame_cb| will be run when the operation
  // completes. The copy will occur to |target|.
  //
  // This may be used as a ContentCaptureSubscription::CaptureCallback.
  void Capture(
      const base::Time& start_time,
      const scoped_refptr<media::VideoFrame>& target,
      const DeliverFrameCallback& deliver_frame_cb);

  // content::WebContentsObserver implementation.
  virtual void DidShowFullscreenWidget(int routing_id) OVERRIDE {
    fullscreen_widget_id_ = routing_id;
    RenewFrameSubscription();
  }

  virtual void DidDestroyFullscreenWidget(int routing_id) OVERRIDE {
    DCHECK_EQ(fullscreen_widget_id_, routing_id);
    fullscreen_widget_id_ = MSG_ROUTING_NONE;
    RenewFrameSubscription();
  }

  virtual void RenderViewReady() OVERRIDE {
    RenewFrameSubscription();
  }

  virtual void AboutToNavigateRenderView(RenderViewHost* rvh) OVERRIDE {
    RenewFrameSubscription();
  }

  virtual void DidNavigateMainFrame(
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) OVERRIDE {
    RenewFrameSubscription();
  }

  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;

 private:
  CaptureMachine(
     const scoped_refptr<base::SequencedTaskRunner>& render_task_runner,
     const scoped_refptr<CaptureOracle>& oracle);

  // Starts observing the web contents, returning false if lookup fails.
  bool StartObservingWebContents(int initial_render_process_id,
                                 int initial_render_view_id);

  // Helper function to determine the view that we are currently tracking.
  RenderWidgetHost* GetTarget();

  // Response callback for RenderWidgetHost::CopyFromBackingStore().
  void DidCopyFromBackingStore(
      const base::Time& start_time,
      const scoped_refptr<media::VideoFrame>& target,
      const DeliverFrameCallback& deliver_frame_cb,
      bool success,
      const SkBitmap& bitmap);

  // Response callback for RWHVP::CopyFromCompositingSurfaceToVideoFrame().
  void DidCopyFromCompositingSurfaceToVideoFrame(
      const base::Time& start_time,
      const DeliverFrameCallback& deliver_frame_cb,
      bool success);

  // Remove the old subscription, and start a new one. This should be called
  // after any change to the WebContents that affects the RenderWidgetHost or
  // attached views.
  void RenewFrameSubscription();

  // The task runner of the thread on which SkBitmap->VideoFrame conversion will
  // occur. Only used when this activity cannot be done on the GPU.
  const scoped_refptr<base::SequencedTaskRunner> render_task_runner_;

  // Makes all the decisions about which frames to copy, and how.
  const scoped_refptr<CaptureOracle> oracle_;

  // Routing ID of any active fullscreen render widget or MSG_ROUTING_NONE
  // otherwise.
  int fullscreen_widget_id_;

  // Last known RenderView size.
  gfx::Size last_view_size_;

  // Responsible for forwarding events from the active RenderWidgetHost to the
  // oracle, and initiating captures accordingly.
  scoped_ptr<ContentCaptureSubscription> subscription_;

  DISALLOW_COPY_AND_ASSIGN(CaptureMachine);
};

// Responsible for logging the effective frame rate.
// TODO(nick): Make this compatible with the push model and hook it back up.
class VideoFrameDeliveryLog {
 public:
  VideoFrameDeliveryLog();

  // Treat |frame_number| as having been delivered, and update the
  // frame rate statistics accordingly.
  void ChronicleFrameDelivery(int frame_number);

 private:
  // The following keep track of and log the effective frame rate whenever
  // verbose logging is turned on.
  base::Time last_frame_rate_log_time_;
  int count_frames_rendered_;
  int last_frame_number_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameDeliveryLog);
};

CaptureOracle::CaptureOracle(media::VideoCaptureDevice::EventHandler* consumer,
                             base::TimeDelta capture_period)
    : capture_period_(capture_period),
      consumer_(consumer),
      frame_number_(0),
      last_delivered_frame_number_(0),
      is_started_(false),
      sampler_(capture_period_, kAcceleratedSubscriberIsSupported,
               kNumRedundantCapturesOfStaticContent) {}

bool CaptureOracle::ObserveEventAndDecideCapture(
      Event event,
      base::Time event_time,
      scoped_refptr<media::VideoFrame>* storage,
      RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback* callback) {
  base::AutoLock guard(lock_);

  if (!consumer_ || !is_started_)
    return false;  // Capture is stopped.

  scoped_refptr<media::VideoFrame> output_buffer =
      consumer_->ReserveOutputBuffer();

  // Record |event| and decide whether it's a good time to capture.
  const bool content_is_dirty = (event == COMPOSITOR_UPDATE ||
                                 event == SOFTWARE_PAINT);
  bool should_sample;
  if (content_is_dirty) {
    frame_number_++;
    should_sample = sampler_.AddEventAndConsiderSampling(event_time);
  } else {
    should_sample = sampler_.IsOverdueForSamplingAt(event_time);
  }

  const char* event_name = (event == TIMER_POLL ? "poll" :
                            (event == COMPOSITOR_UPDATE ? "gpu" :
                             "paint"));

  // Step 3: Consider the various reasons not to initiate a capture.
  if (should_sample && !output_buffer) {
    TRACE_EVENT_INSTANT1("mirroring", "EncodeLimited",
                         TRACE_EVENT_SCOPE_THREAD,
                         "trigger", event_name);
    return false;
  } else if (!should_sample && output_buffer) {
    if (content_is_dirty) {
      // This is a normal and acceptable way to drop a frame. We've hit our
      // capture rate limit: for example, the content is animating at 60fps but
      // we're capturing at 30fps.
      TRACE_EVENT_INSTANT1("mirroring", "FpsRateLimited",
                           TRACE_EVENT_SCOPE_THREAD,
                           "trigger", event_name);
    }
    return false;
  } else if (!should_sample && !output_buffer) {
    // We decided not to capture, but we wouldn't have been able to if we wanted
    // to because no output buffer was available.
    TRACE_EVENT_INSTANT1("mirroring", "NearlyEncodeLimited",
                         TRACE_EVENT_SCOPE_THREAD,
                         "trigger", event_name);
    return false;
  }

  // Step 4: Initiate a capture.
  sampler_.RecordSample();
  TRACE_EVENT_ASYNC_BEGIN2("mirroring", "Capture", output_buffer.get(),
                           "frame_number", frame_number_,
                           "trigger", event_name);
  *storage = output_buffer;
  *callback = base::Bind(&CaptureOracle::DidCaptureFrame,
                         this, output_buffer, frame_number_);
  return true;
}

void CaptureOracle::Start() {
  base::AutoLock guard(lock_);
  is_started_ = true;
}

void CaptureOracle::Stop() {
  base::AutoLock guard(lock_);
  is_started_ = false;
}

void CaptureOracle::ReportError() {
  base::AutoLock guard(lock_);
  if (consumer_)
    consumer_->OnError();
}

void CaptureOracle::InvalidateConsumer() {
  base::AutoLock guard(lock_);

  TRACE_EVENT_INSTANT0("mirroring", "InvalidateConsumer",
                       TRACE_EVENT_SCOPE_THREAD);

  is_started_ = false;
  consumer_ = NULL;
}

void CaptureOracle::DidCaptureFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    int frame_number,
    base::Time timestamp,
    bool success) {
  base::AutoLock guard(lock_);

  TRACE_EVENT_ASYNC_END2("mirroring", "Capture", frame.get(),
                         "success", success,
                         "timestamp", timestamp.ToInternalValue());

  if (!consumer_ || !is_started_)
    return;  // Capture is stopped.

  if (success) {
    // Drop frame if previous frame number is higher or we're trying to deliver
    // a frame with the same timestamp.
    if (last_delivered_frame_number_ > frame_number ||
        last_delivered_frame_timestamp_ == timestamp) {
      LOG(ERROR) << "Frame with same timestamp or out of order delivery. "
                 << "Dropping frame.";
      return;
    }

    if (last_delivered_frame_timestamp_ > timestamp) {
      // We should not get here unless time was adjusted backwards.
      LOG(ERROR) << "Frame with past timestamp (" << timestamp.ToInternalValue()
                 << ") was delivered";
    }

    last_delivered_frame_number_ = frame_number;
    last_delivered_frame_timestamp_ = timestamp;

    consumer_->OnIncomingCapturedVideoFrame(frame, timestamp);
  }
}

bool FrameSubscriber::ShouldCaptureFrame(
    base::Time present_time,
    scoped_refptr<media::VideoFrame>* storage,
    DeliverFrameCallback* deliver_frame_cb) {
  TRACE_EVENT1("mirroring", "FrameSubscriber::ShouldCaptureFrame",
               "instance", this);

  return oracle_->ObserveEventAndDecideCapture(event_type_, present_time,
                                               storage, deliver_frame_cb);
}

ContentCaptureSubscription::ContentCaptureSubscription(
    const RenderWidgetHost& source,
    const scoped_refptr<CaptureOracle>& oracle,
    const CaptureCallback& capture_callback)
    : render_process_id_(source.GetProcess()->GetID()),
      render_view_id_(source.GetRoutingID()),
      paint_subscriber_(CaptureOracle::SOFTWARE_PAINT, oracle),
      timer_subscriber_(CaptureOracle::TIMER_POLL, oracle),
      capture_callback_(capture_callback),
      timer_(true, true) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderWidgetHostViewPort* view =
      RenderWidgetHostViewPort::FromRWHV(source.GetView());

  // Subscribe to accelerated presents. These will be serviced directly by the
  // oracle.
  if (view && kAcceleratedSubscriberIsSupported) {
    scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber(
        new FrameSubscriber(CaptureOracle::COMPOSITOR_UPDATE, oracle));
    view->BeginFrameSubscription(subscriber.Pass());
  }

  // Subscribe to software paint events. This instance will service these by
  // reflecting them back to the CaptureMachine via |capture_callback|.
  registrar_.Add(
      this, content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
      Source<RenderWidgetHost>(&source));

  // Subscribe to timer events. This instance will service these as well.
  timer_.Start(FROM_HERE, oracle->capture_period(),
               base::Bind(&ContentCaptureSubscription::OnTimer,
                          base::Unretained(this)));
}

ContentCaptureSubscription::~ContentCaptureSubscription() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (kAcceleratedSubscriberIsSupported) {
    RenderViewHost* source = RenderViewHost::FromID(render_process_id_,
                                                    render_view_id_);
    if (source) {
      RenderWidgetHostViewPort* view =
          RenderWidgetHostViewPort::FromRWHV(source->GetView());
      if (view)
        view->EndFrameSubscription();
    }
  }
}

void ContentCaptureSubscription::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE, type);

  RenderWidgetHostImpl* rwh =
      RenderWidgetHostImpl::From(Source<RenderWidgetHost>(source).ptr());

  // This message occurs on window resizes and visibility changes even when
  // accelerated compositing is active, so we need to filter out these cases.
  if (!rwh || !rwh->GetView() || rwh->is_accelerated_compositing_active())
    return;

  TRACE_EVENT1("mirroring", "ContentCaptureSubscription::Observe",
               "instance", this);

  base::Closure copy_done_callback;
  scoped_refptr<media::VideoFrame> frame;
  RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback deliver_frame_cb;
  const base::Time start_time = base::Time::Now();
  if (paint_subscriber_.ShouldCaptureFrame(start_time,
                                           &frame,
                                           &deliver_frame_cb)) {
    // This message happens just before paint. If we post a task to do the copy,
    // it should run soon after the paint.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(capture_callback_, start_time, frame, deliver_frame_cb));
  }
}

void ContentCaptureSubscription::OnTimer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("mirroring", "ContentCaptureSubscription::OnTimer");

  scoped_refptr<media::VideoFrame> frame;
  RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback deliver_frame_cb;

  const base::Time start_time = base::Time::Now();
  if (timer_subscriber_.ShouldCaptureFrame(start_time,
                                           &frame,
                                           &deliver_frame_cb)) {
    capture_callback_.Run(start_time, frame, deliver_frame_cb);
  }
}

void RenderVideoFrame(const SkBitmap& input,
                      const scoped_refptr<media::VideoFrame>& output,
                      const base::Callback<void(bool)>& done_cb) {
  base::ScopedClosureRunner failure_handler(base::Bind(done_cb, false));

  SkAutoLockPixels locker(input);

  // Sanity-check the captured bitmap.
  if (input.empty() ||
      !input.readyToDraw() ||
      input.config() != SkBitmap::kARGB_8888_Config ||
      input.width() < 2 || input.height() < 2) {
    DVLOG(1) << "input unacceptable (size="
             << input.getSize()
             << ", ready=" << input.readyToDraw()
             << ", config=" << input.config() << ')';
    return;
  }

  // Sanity-check the output buffer.
  if (output->format() != media::VideoFrame::YV12) {
    NOTREACHED();
    return;
  }

  // Calculate the width and height of the content region in the |output|, based
  // on the aspect ratio of |input|.
  gfx::Rect region_in_frame = ComputeYV12LetterboxRegion(
      output->coded_size(), gfx::Size(input.width(), input.height()));

  // Scale the bitmap to the required size, if necessary.
  SkBitmap scaled_bitmap;
  if (input.width() != region_in_frame.width() ||
      input.height() != region_in_frame.height()) {
    TRACE_EVENT_ASYNC_STEP0("mirroring", "Capture", output.get(), "Scale");
    scaled_bitmap = skia::ImageOperations::Resize(
        input, skia::ImageOperations::RESIZE_BOX,
        region_in_frame.width(), region_in_frame.height());
  } else {
    scaled_bitmap = input;
  }

  TRACE_EVENT_ASYNC_STEP0("mirroring", "Capture", output.get(), "YUV");
  {
    SkAutoLockPixels scaled_bitmap_locker(scaled_bitmap);

    media::CopyRGBToVideoFrame(
        reinterpret_cast<uint8*>(scaled_bitmap.getPixels()),
        scaled_bitmap.rowBytes(),
        region_in_frame,
        output);
  }

  // The result is now ready.
  failure_handler.Release();
  done_cb.Run(true);
}

VideoFrameDeliveryLog::VideoFrameDeliveryLog()
    : last_frame_rate_log_time_(),
      count_frames_rendered_(0),
      last_frame_number_(0) {
}

void VideoFrameDeliveryLog::ChronicleFrameDelivery(int frame_number) {
  // Log frame rate, if verbose logging is turned on.
  static const base::TimeDelta kFrameRateLogInterval =
      base::TimeDelta::FromSeconds(10);
  const base::Time now = base::Time::Now();
  if (last_frame_rate_log_time_.is_null()) {
    last_frame_rate_log_time_ = now;
    count_frames_rendered_ = 0;
    last_frame_number_ = frame_number;
  } else {
    ++count_frames_rendered_;
    const base::TimeDelta elapsed = now - last_frame_rate_log_time_;
    if (elapsed >= kFrameRateLogInterval) {
      const double measured_fps =
          count_frames_rendered_ / elapsed.InSecondsF();
      const int frames_elapsed = frame_number - last_frame_number_;
      const int count_frames_dropped = frames_elapsed - count_frames_rendered_;
      DCHECK_LE(0, count_frames_dropped);
      UMA_HISTOGRAM_PERCENTAGE(
          "TabCapture.FrameDropPercentage",
          (count_frames_dropped * 100 + frames_elapsed / 2) / frames_elapsed);
      UMA_HISTOGRAM_COUNTS(
          "TabCapture.FrameRate",
          static_cast<int>(measured_fps));
      VLOG(1) << "Current measured frame rate for "
              << "WebContentsVideoCaptureDevice is " << measured_fps << " FPS.";
      last_frame_rate_log_time_ = now;
      count_frames_rendered_ = 0;
      last_frame_number_ = frame_number;
    }
  }
}

// static
scoped_ptr<CaptureMachine> CaptureMachine::Create(
    int render_process_id,
    int render_view_id,
    const scoped_refptr<base::SequencedTaskRunner>& render_task_runner,
    const scoped_refptr<CaptureOracle>& oracle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(render_task_runner);
  DCHECK(oracle);
  scoped_ptr<CaptureMachine> machine(
      new CaptureMachine(render_task_runner, oracle));

  if (!machine->StartObservingWebContents(render_process_id, render_view_id))
    machine.reset();

  return machine.Pass();
}

CaptureMachine::CaptureMachine(
    const scoped_refptr<base::SequencedTaskRunner>& render_task_runner,
    const scoped_refptr<CaptureOracle>& oracle)
    : render_task_runner_(render_task_runner),
      oracle_(oracle),
      fullscreen_widget_id_(MSG_ROUTING_NONE) {}

CaptureMachine::~CaptureMachine() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsMessageLoopValid(BrowserThread::UI));

  // Stop observing the web contents.
  subscription_.reset();
  if (web_contents()) {
    web_contents()->DecrementCapturerCount();
    Observe(NULL);
  }
}

void CaptureMachine::Capture(
    const base::Time& start_time,
    const scoped_refptr<media::VideoFrame>& target,
    const DeliverFrameCallback& deliver_frame_cb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderWidgetHost* rwh = GetTarget();
  RenderWidgetHostViewPort* view =
      rwh ? RenderWidgetHostViewPort::FromRWHV(rwh->GetView()) : NULL;
  if (!view || !rwh) {
    deliver_frame_cb.Run(base::Time(), false);
    return;
  }

  gfx::Size video_size = target->coded_size();
  gfx::Size view_size = view->GetViewBounds().size();
  gfx::Size fitted_size;
  if (!view_size.IsEmpty()) {
    fitted_size = ComputeYV12LetterboxRegion(video_size, view_size).size();
  }
  if (view_size != last_view_size_) {
    last_view_size_ = view_size;

    // Measure the number of kilopixels.
    UMA_HISTOGRAM_COUNTS_10000(
        "TabCapture.ViewChangeKiloPixels",
        view_size.width() * view_size.height() / 1024);
  }

  if (!view->IsSurfaceAvailableForCopy()) {
    // Fallback to the more expensive renderer-side copy if the surface and
    // backing store are not accessible.
    rwh->GetSnapshotFromRenderer(
        gfx::Rect(),
        base::Bind(&CaptureMachine::DidCopyFromBackingStore, this->AsWeakPtr(),
                   start_time, target, deliver_frame_cb));
  } else if (view->CanCopyToVideoFrame()) {
    view->CopyFromCompositingSurfaceToVideoFrame(
        gfx::Rect(view_size),
        target,
        base::Bind(&CaptureMachine::DidCopyFromCompositingSurfaceToVideoFrame,
                   this->AsWeakPtr(), start_time, deliver_frame_cb));
  } else {
    rwh->CopyFromBackingStore(
        gfx::Rect(),
        fitted_size,  // Size here is a request not always honored.
        base::Bind(&CaptureMachine::DidCopyFromBackingStore, this->AsWeakPtr(),
                   start_time, target, deliver_frame_cb));
  }
}

bool CaptureMachine::StartObservingWebContents(int initial_render_process_id,
                                               int initial_render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Look-up the RenderViewHost and, from that, the WebContents that wraps it.
  // If successful, begin observing the WebContents instance.
  //
  // Why this can be unsuccessful: The request for mirroring originates in a
  // render process, and this request is based on the current RenderView
  // associated with a tab.  However, by the time we get up-and-running here,
  // there have been multiple back-and-forth IPCs between processes, as well as
  // a bit of indirection across threads.  It's easily possible that, in the
  // meantime, the original RenderView may have gone away.
  RenderViewHost* const rvh =
      RenderViewHost::FromID(initial_render_process_id,
                             initial_render_view_id);
  DVLOG_IF(1, !rvh) << "RenderViewHost::FromID("
                    << initial_render_process_id << ", "
                    << initial_render_view_id << ") returned NULL.";
  Observe(rvh ? WebContents::FromRenderViewHost(rvh) : NULL);

  WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents());
  if (contents) {
    contents->IncrementCapturerCount();
    fullscreen_widget_id_ = contents->GetFullscreenWidgetRoutingID();
    RenewFrameSubscription();
    return true;
  }

  DVLOG(1) << "WebContents::FromRenderViewHost(" << rvh << ") returned NULL.";
  return false;
}

void CaptureMachine::WebContentsDestroyed(WebContents* web_contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  subscription_.reset();
  web_contents->DecrementCapturerCount();
  oracle_->ReportError();
}

RenderWidgetHost* CaptureMachine::GetTarget() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!web_contents())
    return NULL;

  RenderWidgetHost* rwh = NULL;
  if (fullscreen_widget_id_ != MSG_ROUTING_NONE) {
    RenderProcessHost* process = web_contents()->GetRenderProcessHost();
    rwh = process ? process->GetRenderWidgetHostByID(fullscreen_widget_id_)
                  : NULL;
  } else {
    rwh = web_contents()->GetRenderViewHost();
  }

  return rwh;
}

void CaptureMachine::DidCopyFromBackingStore(
    const base::Time& start_time,
    const scoped_refptr<media::VideoFrame>& target,
    const DeliverFrameCallback& deliver_frame_cb,
    bool success,
    const SkBitmap& bitmap) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::Time now = base::Time::Now();
  if (success) {
    UMA_HISTOGRAM_TIMES("TabCapture.CopyTimeBitmap", now - start_time);
    TRACE_EVENT_ASYNC_STEP0("mirroring", "Capture", target.get(), "Render");
    render_task_runner_->PostTask(FROM_HERE, base::Bind(
        &RenderVideoFrame, bitmap, target,
        base::Bind(deliver_frame_cb, start_time)));
  } else {
    // Capture can fail due to transient issues, so just skip this frame.
    DVLOG(1) << "CopyFromBackingStore failed; skipping frame.";
    deliver_frame_cb.Run(start_time, false);
  }
}

void CaptureMachine::DidCopyFromCompositingSurfaceToVideoFrame(
    const base::Time& start_time,
    const DeliverFrameCallback& deliver_frame_cb,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::Time now = base::Time::Now();

  if (success) {
    UMA_HISTOGRAM_TIMES("TabCapture.CopyTimeVideoFrame", now - start_time);
  } else {
    // Capture can fail due to transient issues, so just skip this frame.
    DVLOG(1) << "CopyFromCompositingSurface failed; skipping frame.";
  }
  deliver_frame_cb.Run(start_time, success);
}

void CaptureMachine::RenewFrameSubscription() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Always destroy the old subscription before creating a new one.
  subscription_.reset();

  RenderWidgetHost* rwh = GetTarget();
  if (!rwh || !rwh->GetView())
    return;

  subscription_.reset(new ContentCaptureSubscription(*rwh, oracle_,
      base::Bind(&CaptureMachine::Capture, this->AsWeakPtr())));
}

void DeleteCaptureMachineOnUIThread(
    scoped_ptr<CaptureMachine> capture_machine) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  capture_machine.reset();
}

}  // namespace

// The "meat" of the video capture implementation, which is a ref-counted class.
// Separating this from the "shell class" WebContentsVideoCaptureDevice allows
// safe destruction without needing to block any threads (e.g., the IO
// BrowserThread).
//
// WebContentsVideoCaptureDevice::Impl manages a simple state machine and the
// pipeline (see notes at top of this file).  It times the start of successive
// captures and facilitates the processing of each through the stages of the
// pipeline.
class WebContentsVideoCaptureDevice::Impl : public base::SupportsWeakPtr<Impl> {
 public:

  Impl(int render_process_id, int render_view_id);
  virtual ~Impl();

  // Asynchronous requests to change WebContentsVideoCaptureDevice::Impl state.
  void Allocate(int width,
                int height,
                int frame_rate,
                media::VideoCaptureDevice::EventHandler* consumer);
  void Start();
  void Stop();
  void DeAllocate();

 private:

  // Flag indicating current state.
  enum State {
    kIdle,
    kAllocated,
    kCapturing,
    kError
  };

  void TransitionStateTo(State next_state);

  // Stops capturing and notifies consumer_ of an error state.
  void Error();

  // Called in response to CaptureMachine::Create that runs on the UI thread.
  // It will assign the capture machine to the Impl class if it still exists
  // otherwise it will post a task to delete CaptureMachine on the UI thread.
  static void AssignCaptureMachine(
      base::WeakPtr<WebContentsVideoCaptureDevice::Impl> impl,
      scoped_ptr<CaptureMachine> capture_machine);

  // Tracks that all activity occurs on the media stream manager's thread.
  base::ThreadChecker thread_checker_;

  // These values identify the starting view that will be captured. After
  // capture starts, the target view IDs will change as navigation occurs, and
  // so these values are not relevant after the initial bootstrapping.
  const int initial_render_process_id_;
  const int initial_render_view_id_;

  // Our event handler, which gobbles the frames we capture.
  VideoCaptureDevice::EventHandler* consumer_;

  // Current lifecycle state.
  State state_;

  // A dedicated worker thread for doing image operations. Started/joined here,
  // but used by the CaptureMachine.
  base::Thread render_thread_;

  // Tracks the CaptureMachine that's doing work on our behalf on the UI thread.
  // This value should never be dereferenced by this class, other than to
  // create and destroy it on the UI thread.
  scoped_ptr<CaptureMachine> capture_machine_;

  // Our thread-safe capture oracle which serves as the gateway to the video
  // capture pipeline. Besides the WCVCD itself, it is the only component of the
  // system with direct access to |consumer_|.
  scoped_refptr<CaptureOracle> oracle_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

WebContentsVideoCaptureDevice::Impl::Impl(int render_process_id,
                                          int render_view_id)
    : initial_render_process_id_(render_process_id),
      initial_render_view_id_(render_view_id),
      consumer_(NULL),
      state_(kIdle),
      render_thread_("WebContentsVideo_RenderThread") {
}

void WebContentsVideoCaptureDevice::Impl::Allocate(
    int width,
    int height,
    int frame_rate,
    VideoCaptureDevice::EventHandler* consumer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kIdle) {
    DVLOG(1) << "Allocate() invoked when not in state Idle.";
    return;
  }

  if (frame_rate <= 0) {
    DVLOG(1) << "invalid frame_rate: " << frame_rate;
    consumer->OnError();
    return;
  }

  if (!render_thread_.Start()) {
    DVLOG(1) << "Failed to spawn render thread.";
    consumer->OnError();
    return;
  }

  // Frame dimensions must each be a positive, even integer, since the consumer
  // wants (or will convert to) YUV420.
  width = MakeEven(width);
  height = MakeEven(height);
  if (width < kMinFrameWidth || height < kMinFrameHeight) {
    DVLOG(1) << "invalid width (" << width << ") and/or height ("
             << height << ")";
    consumer->OnError();
    return;
  }

  // Initialize capture settings which will be consistent for the
  // duration of the capture.
  media::VideoCaptureCapability settings = { 0 };

  settings.width = width;
  settings.height = height;
  settings.frame_rate = frame_rate;
  // Note: the value of |settings.color| doesn't matter if we use only the
  // VideoFrame based methods on |consumer|.
  settings.color = media::VideoCaptureCapability::kI420;
  settings.expected_capture_delay = 0;
  settings.interlaced = false;

  base::TimeDelta capture_period = base::TimeDelta::FromMicroseconds(
      1000000.0 / settings.frame_rate + 0.5);

  consumer_ = consumer;
  consumer_->OnFrameInfo(settings);
  oracle_ = new CaptureOracle(consumer_, capture_period);

  // Allocates the CaptureMachine. The CaptureMachine will be tracking render
  // view swapping over its lifetime, and we don't want to lose our reference to
  // the current render view by starting over with the stale
  // |initial_render_view_id_|.
  DCHECK(!capture_machine_.get());
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CaptureMachine::Create,
                 initial_render_process_id_,
                 initial_render_view_id_,
                 render_thread_.message_loop_proxy(), oracle_),
      base::Bind(&Impl::AssignCaptureMachine, AsWeakPtr()));

  TransitionStateTo(kAllocated);
}

void WebContentsVideoCaptureDevice::Impl::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kAllocated) {
    return;
  }

  TransitionStateTo(kCapturing);

  oracle_->Start();
}

// static
void WebContentsVideoCaptureDevice::Impl::AssignCaptureMachine(
    base::WeakPtr<WebContentsVideoCaptureDevice::Impl> impl,
    scoped_ptr<CaptureMachine> capture_machine) {
  DCHECK(!impl || impl->thread_checker_.CalledOnValidThread());

  if (!impl) {
    // If WCVD::Impl was destroyed before we got back on it's thread and
    // capture_machine is not NULL, then we need to return to the UI thread to
    // safely cleanup the CaptureMachine.
    if (capture_machine) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE, base::Bind(
              &DeleteCaptureMachineOnUIThread, base::Passed(&capture_machine)));
      return;
    }
  } else if (!capture_machine) {
    impl->Error();
  } else {
    impl->capture_machine_ = capture_machine.Pass();
  }
}

void WebContentsVideoCaptureDevice::Impl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kCapturing) {
    return;
  }
  oracle_->Stop();

  TransitionStateTo(kAllocated);
}

void WebContentsVideoCaptureDevice::Impl::DeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == kCapturing) {
    Stop();
  }
  if (state_ == kAllocated) {
    // |consumer_| is about to be deleted, so we mustn't use it anymore.
    oracle_->InvalidateConsumer();
    consumer_ = NULL;
    oracle_ = NULL;
    render_thread_.Stop();

    TransitionStateTo(kIdle);
  }
}

WebContentsVideoCaptureDevice::Impl::~Impl() {
  // There is still a capture pipeline running that is checking in with the
  // oracle, and processing captures that are already started in flight. That
  // pipeline must be shut down asynchronously, on the UI thread.
  if (capture_machine_) {
    // The task that is posted to the UI thread might not run if we are shutting
    // down, so we transfer ownership of CaptureMachine to the closure so that
    // it is still cleaned up when the closure is deleted.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(
            &DeleteCaptureMachineOnUIThread, base::Passed(&capture_machine_)));
  }

  DCHECK(!capture_machine_) << "Cleanup on UI thread did not happen.";
  DCHECK(!consumer_) << "Device not DeAllocated -- possible data race.";
  DVLOG(1) << "WebContentsVideoCaptureDevice::Impl@" << this << " destroying.";
}

void WebContentsVideoCaptureDevice::Impl::TransitionStateTo(State next_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

#ifndef NDEBUG
  static const char* kStateNames[] = {
    "Idle", "Allocated", "Capturing", "Error"
  };
  DVLOG(1) << "State change: " << kStateNames[state_]
           << " --> " << kStateNames[next_state];
#endif

  state_ = next_state;
}

void WebContentsVideoCaptureDevice::Impl::Error() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == kIdle)
    return;

  if (consumer_)
    consumer_->OnError();

  DeAllocate();
  TransitionStateTo(kError);
}

WebContentsVideoCaptureDevice::WebContentsVideoCaptureDevice(
    const media::VideoCaptureDevice::Name& name,
    int render_process_id,
    int render_view_id)
    : device_name_(name),
      impl_(new WebContentsVideoCaptureDevice::Impl(render_process_id,
                                                    render_view_id)) {}

WebContentsVideoCaptureDevice::~WebContentsVideoCaptureDevice() {
  DVLOG(2) << "WebContentsVideoCaptureDevice@" << this << " destroying.";
}

// static
media::VideoCaptureDevice* WebContentsVideoCaptureDevice::Create(
    const std::string& device_id) {
  // Parse device_id into render_process_id and render_view_id.
  int render_process_id = -1;
  int render_view_id = -1;
  if (!WebContentsCaptureUtil::ExtractTabCaptureTarget(device_id,
                                                       &render_process_id,
                                                       &render_view_id))
    return NULL;

  media::VideoCaptureDevice::Name name;
  base::SStringPrintf(&name.device_name,
                      "WebContents[%.*s]",
                      static_cast<int>(device_id.size()), device_id.data());
  name.unique_id = device_id;

  return new WebContentsVideoCaptureDevice(
      name, render_process_id, render_view_id);
}

void WebContentsVideoCaptureDevice::Allocate(
    int width, int height, int frame_rate,
    VideoCaptureDevice::EventHandler* consumer) {
  impl_->Allocate(width, height, frame_rate, consumer);
}

void WebContentsVideoCaptureDevice::Start() {
  impl_->Start();
}

void WebContentsVideoCaptureDevice::Stop() {
  impl_->Stop();
}

void WebContentsVideoCaptureDevice::DeAllocate() {
  impl_->DeAllocate();
}

const media::VideoCaptureDevice::Name&
WebContentsVideoCaptureDevice::device_name() {
  return device_name_;
}

SmoothEventSampler::SmoothEventSampler(base::TimeDelta capture_period,
                                       bool events_are_reliable,
                                       int redundant_capture_goal)
    :  events_are_reliable_(events_are_reliable),
       capture_period_(capture_period),
       redundant_capture_goal_(redundant_capture_goal),
       token_bucket_capacity_(capture_period + capture_period / 2),
       overdue_sample_count_(0),
       token_bucket_(token_bucket_capacity_) {
  DCHECK_GT(capture_period_.InMicroseconds(), 0);
}

bool SmoothEventSampler::AddEventAndConsiderSampling(base::Time event_time) {
  DCHECK(!event_time.is_null());

  // Add tokens to the bucket based on advancement in time.  Then, re-bound the
  // number of tokens in the bucket.  Overflow occurs when there is too much
  // time between events (a common case), or when RecordSample() is not being
  // called often enough (a bug).  On the other hand, if RecordSample() is being
  // called too often (e.g., as a reaction to IsOverdueForSamplingAt()), the
  // bucket will underflow.
  if (!current_event_.is_null()) {
    if (current_event_ < event_time) {
      token_bucket_ += event_time - current_event_;
      if (token_bucket_ > token_bucket_capacity_)
        token_bucket_ = token_bucket_capacity_;
    }
    // Side note: If the system clock is reset, causing |current_event_| to be
    // greater than |event_time|, everything here will simply gracefully adjust.
    if (token_bucket_ < base::TimeDelta())
      token_bucket_ = base::TimeDelta();
    TRACE_COUNTER1("mirroring",
                   "MirroringTokenBucketUsec", token_bucket_.InMicroseconds());
  }
  current_event_ = event_time;

  // Return true if one capture period's worth of tokens are in the bucket.
  return token_bucket_ >= capture_period_;
}

void SmoothEventSampler::RecordSample() {
  token_bucket_ -= capture_period_;
  TRACE_COUNTER1("mirroring",
                 "MirroringTokenBucketUsec", token_bucket_.InMicroseconds());
  if (HasUnrecordedEvent()) {
    last_sample_ = current_event_;
    overdue_sample_count_ = 0;
  } else {
    ++overdue_sample_count_;
  }
}

bool SmoothEventSampler::IsOverdueForSamplingAt(base::Time event_time) const {
  DCHECK(!event_time.is_null());

  // If we don't get events on compositor updates on this platform, then we
  // don't reliably know whether we're dirty.
  if (events_are_reliable_) {
    if (!HasUnrecordedEvent() &&
        overdue_sample_count_ >= redundant_capture_goal_) {
      return false;  // Not dirty.
    }
  }

  // If we're dirty but not yet old, then we've recently gotten updates, so we
  // won't request a sample just yet.
  base::TimeDelta dirty_interval = event_time - last_sample_;
  if (dirty_interval < capture_period_ * 4)
    return false;
  else
    return true;
}

bool SmoothEventSampler::HasUnrecordedEvent() const {
  return !current_event_.is_null() && current_event_ != last_sample_;
}

}  // namespace content
