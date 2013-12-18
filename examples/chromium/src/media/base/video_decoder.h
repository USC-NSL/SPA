// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_DECODER_H_
#define MEDIA_BASE_VIDEO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "ui/gfx/size.h"

namespace media {

class DemuxerStream;
class VideoFrame;

class MEDIA_EXPORT VideoDecoder {
 public:
  // Status codes for read operations on VideoDecoder.
  enum Status {
    kOk,  // Everything went as planned.
    kDecodeError,  // Decoding error happened.
    kDecryptError  // Decrypting error happened.
  };

  VideoDecoder();
  virtual ~VideoDecoder();

  // Initializes a VideoDecoder with the given DemuxerStream, executing the
  // |status_cb| upon completion.
  // |statistics_cb| is used to update the global pipeline statistics.
  //
  // Note:
  // 1) The VideoDecoder will be reinitialized if it was initialized before.
  //    Upon reinitialization, all internal buffered frames will be dropped.
  // 2) This method should not be called during any pending read, reset or stop.
  // 3) No VideoDecoder calls except for Stop() should be made before
  //    |status_cb| is executed.
  // 4) DemuxerStream should not be accessed after the VideoDecoder is stopped.
  //
  // TODO(xhwang): Make all VideoDecoder implementations reinitializable.
  // See http://crbug.com/233608
  virtual void Initialize(DemuxerStream* stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) = 0;

  // Requests a frame to be decoded. The status of the decoder and decoded frame
  // are returned via the provided callback. Only one read may be in flight at
  // any given time.
  //
  // Implementations guarantee that the callback will not be called from within
  // this method.
  //
  // If the returned status is not kOk, some error has occurred in the video
  // decoder. In this case, the returned frame should always be NULL.
  //
  // Otherwise, the video decoder is in good shape. In this case, Non-NULL
  // frames contain decoded video data or may indicate the end of the stream.
  // NULL video frames indicate an aborted read. This can happen if the
  // DemuxerStream gets flushed and doesn't have any more data to return.
  typedef base::Callback<void(Status, const scoped_refptr<VideoFrame>&)> ReadCB;
  virtual void Read(const ReadCB& read_cb) = 0;

  // Resets decoder state, fulfilling all pending ReadCB and dropping extra
  // queued decoded data. After this call, the decoder is back to an initialized
  // clean state.
  // Note: No VideoDecoder calls should be made before |closure| is executed.
  virtual void Reset(const base::Closure& closure) = 0;

  // Stops decoder, fires any pending callbacks and sets the decoder to an
  // uninitialized state. A VideoDecoder cannot be re-initialized after it has
  // been stopped.
  // Note that if Initialize() has been called, Stop() must be called and
  // complete before deleting the decoder.
  virtual void Stop(const base::Closure& closure) = 0;

  // Returns true if the output format has an alpha channel. Most formats do not
  // have alpha so the default is false. Override and return true for decoders
  // that return formats with an alpha channel.
  virtual bool HasAlpha() const;

  // Returns true if the decoder currently has the ability to decode and return
  // a VideoFrame. Most implementations can allocate a new VideoFrame and hence
  // this will always return true. Override and return false for decoders that
  // use a fixed set of VideoFrames for decoding.
  virtual bool HasOutputFrameAvailable() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDecoder);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_DECODER_H_
