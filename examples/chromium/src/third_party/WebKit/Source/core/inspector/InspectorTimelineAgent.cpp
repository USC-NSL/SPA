/*
* Copyright (C) 2013 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "core/inspector/InspectorTimelineAgent.h"

#include "InspectorFrontend.h"
#include "core/dom/Event.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorCounters.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorMemoryAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/TimelineRecordFactory.h"
#include "core/inspector/TimelineTraceEventProcessor.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/DOMWindow.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/platform/MemoryUsageSupport.h"
#include "core/platform/graphics/IntRect.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/platform/network/ResourceResponse.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderView.h"
#include "core/xml/XMLHttpRequest.h"

#include <wtf/CurrentTime.h>

namespace WebCore {

namespace TimelineAgentState {
static const char timelineAgentEnabled[] = "timelineAgentEnabled";
static const char timelineMaxCallStackDepth[] = "timelineMaxCallStackDepth";
static const char includeDomCounters[] = "includeDomCounters";
static const char includeNativeMemoryStatistics[] = "includeNativeMemoryStatistics";
}

// Must be kept in sync with WebInspector.TimelineModel.RecordType in TimelineModel.js
namespace TimelineRecordType {
static const char Program[] = "Program";

static const char EventDispatch[] = "EventDispatch";
static const char BeginFrame[] = "BeginFrame";
static const char ScheduleStyleRecalculation[] = "ScheduleStyleRecalculation";
static const char RecalculateStyles[] = "RecalculateStyles";
static const char InvalidateLayout[] = "InvalidateLayout";
static const char Layout[] = "Layout";
static const char Paint[] = "Paint";
static const char ScrollLayer[] = "ScrollLayer";
static const char ResizeImage[] = "ResizeImage";
static const char CompositeLayers[] = "CompositeLayers";

static const char ParseHTML[] = "ParseHTML";

static const char TimerInstall[] = "TimerInstall";
static const char TimerRemove[] = "TimerRemove";
static const char TimerFire[] = "TimerFire";

static const char EvaluateScript[] = "EvaluateScript";

static const char MarkLoad[] = "MarkLoad";
static const char MarkDOMContent[] = "MarkDOMContent";

static const char TimeStamp[] = "TimeStamp";
static const char Time[] = "Time";
static const char TimeEnd[] = "TimeEnd";

static const char ScheduleResourceRequest[] = "ScheduleResourceRequest";
static const char ResourceSendRequest[] = "ResourceSendRequest";
static const char ResourceReceiveResponse[] = "ResourceReceiveResponse";
static const char ResourceReceivedData[] = "ResourceReceivedData";
static const char ResourceFinish[] = "ResourceFinish";

static const char XHRReadyStateChange[] = "XHRReadyStateChange";
static const char XHRLoad[] = "XHRLoad";

static const char FunctionCall[] = "FunctionCall";
static const char GCEvent[] = "GCEvent";

static const char RequestAnimationFrame[] = "RequestAnimationFrame";
static const char CancelAnimationFrame[] = "CancelAnimationFrame";
static const char FireAnimationFrame[] = "FireAnimationFrame";

static const char WebSocketCreate[] = "WebSocketCreate";
static const char WebSocketSendHandshakeRequest[] = "WebSocketSendHandshakeRequest";
static const char WebSocketReceiveHandshakeResponse[] = "WebSocketReceiveHandshakeResponse";
static const char WebSocketDestroy[] = "WebSocketDestroy";

// Event names visible to other modules.
const char DecodeImage[] = "DecodeImage";
const char Rasterize[] = "Rasterize";
}

static Frame* frameForScriptExecutionContext(ScriptExecutionContext* context)
{
    Frame* frame = 0;
    if (context->isDocument())
        frame = toDocument(context)->frame();
    return frame;
}

static bool eventHasListeners(const AtomicString& eventType, DOMWindow* window, Node* node, const EventPath& eventPath)
{
    if (window && window->hasEventListeners(eventType))
        return true;

    if (node->hasEventListeners(eventType))
        return true;

    for (size_t i = 0; i < eventPath.size(); i++) {
        if (eventPath[i]->node()->hasEventListeners(eventType))
            return true;
    }

    return false;
}

void TimelineTimeConverter::reset()
{
    m_startOffset = monotonicallyIncreasingTime() - currentTime();
}

void InspectorTimelineAgent::pushGCEventRecords()
{
    if (!m_gcEvents.size())
        return;

    GCEvents events = m_gcEvents;
    m_gcEvents.clear();
    for (GCEvents::iterator i = events.begin(); i != events.end(); ++i) {
        RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(m_timeConverter.fromMonotonicallyIncreasingTime(i->startTime), m_maxCallStackDepth);
        record->setObject("data", TimelineRecordFactory::createGCEventData(i->collectedBytes));
        record->setNumber("endTime", m_timeConverter.fromMonotonicallyIncreasingTime(i->endTime));
        addRecordToTimeline(record.release(), TimelineRecordType::GCEvent);
    }
}

void InspectorTimelineAgent::didGC(double startTime, double endTime, size_t collectedBytesCount)
{
    m_gcEvents.append(GCEvent(startTime, endTime, collectedBytesCount));
}

InspectorTimelineAgent::~InspectorTimelineAgent()
{
    clearFrontend();
}

void InspectorTimelineAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->timeline();
}

void InspectorTimelineAgent::clearFrontend()
{
    ErrorString error;
    stop(&error);
    m_frontend = 0;
}

void InspectorTimelineAgent::restore()
{
    if (m_state->getBoolean(TimelineAgentState::timelineAgentEnabled)) {
        m_maxCallStackDepth = m_state->getLong(TimelineAgentState::timelineMaxCallStackDepth);
        ErrorString error;
        bool includeDomCounters = m_state->getBoolean(TimelineAgentState::includeDomCounters);
        bool includeNativeMemoryStatistics = m_state->getBoolean(TimelineAgentState::includeNativeMemoryStatistics);
        start(&error, &m_maxCallStackDepth, &includeDomCounters, &includeNativeMemoryStatistics);
    }
}

void InspectorTimelineAgent::start(ErrorString*, const int* maxCallStackDepth, const bool* includeDomCounters, const bool* includeNativeMemoryStatistics)
{
    if (!m_frontend)
        return;

    if (maxCallStackDepth && *maxCallStackDepth >= 0)
        m_maxCallStackDepth = *maxCallStackDepth;
    else
        m_maxCallStackDepth = 5;
    m_state->setLong(TimelineAgentState::timelineMaxCallStackDepth, m_maxCallStackDepth);
    m_state->setBoolean(TimelineAgentState::includeDomCounters, includeDomCounters && *includeDomCounters);
    m_state->setBoolean(TimelineAgentState::includeNativeMemoryStatistics, includeNativeMemoryStatistics && *includeNativeMemoryStatistics);
    m_timeConverter.reset();

    m_instrumentingAgents->setInspectorTimelineAgent(this);
    ScriptGCEvent::addEventListener(this);
    m_state->setBoolean(TimelineAgentState::timelineAgentEnabled, true);
    if (m_client && m_pageAgent)
        m_traceEventProcessor = adoptRef(new TimelineTraceEventProcessor(m_weakFactory.createWeakPtr(), m_client));
}

void InspectorTimelineAgent::stop(ErrorString*)
{
    if (!m_state->getBoolean(TimelineAgentState::timelineAgentEnabled))
        return;

    m_traceEventProcessor->shutdown();
    m_traceEventProcessor.clear();
    m_weakFactory.revokeAll();
    m_instrumentingAgents->setInspectorTimelineAgent(0);
    ScriptGCEvent::removeEventListener(this);

    clearRecordStack();
    m_gcEvents.clear();

    m_state->setBoolean(TimelineAgentState::timelineAgentEnabled, false);
}

void InspectorTimelineAgent::didBeginFrame()
{
    TRACE_EVENT_INSTANT0("webkit", InstrumentationEvents::BeginFrame);
    m_pendingFrameRecord = TimelineRecordFactory::createGenericRecord(timestamp(), 0);
}

void InspectorTimelineAgent::didCancelFrame()
{
    m_pendingFrameRecord.clear();
}

bool InspectorTimelineAgent::willCallFunction(ScriptExecutionContext* context, const String& scriptName, int scriptLine)
{
    pushCurrentRecord(TimelineRecordFactory::createFunctionCallData(scriptName, scriptLine), TimelineRecordType::FunctionCall, true, frameForScriptExecutionContext(context));
    return true;
}

void InspectorTimelineAgent::didCallFunction()
{
    didCompleteCurrentRecord(TimelineRecordType::FunctionCall);
}

bool InspectorTimelineAgent::willDispatchEvent(Document* document, const Event& event, DOMWindow* window, Node* node, const EventPath& eventPath)
{
    if (!eventHasListeners(event.type(), window, node, eventPath))
       return false;

    pushCurrentRecord(TimelineRecordFactory::createEventDispatchData(event), TimelineRecordType::EventDispatch, false, document->frame());
    return true;
}

bool InspectorTimelineAgent::willDispatchEventOnWindow(const Event& event, DOMWindow* window)
{
    if (!window->hasEventListeners(event.type()))
        return false;
    pushCurrentRecord(TimelineRecordFactory::createEventDispatchData(event), TimelineRecordType::EventDispatch, false, window->frame());
    return true;
}

void InspectorTimelineAgent::didDispatchEvent()
{
    didCompleteCurrentRecord(TimelineRecordType::EventDispatch);
}

void InspectorTimelineAgent::didDispatchEventOnWindow()
{
    didDispatchEvent();
}

void InspectorTimelineAgent::didInvalidateLayout(Frame* frame)
{
    appendRecord(InspectorObject::create(), TimelineRecordType::InvalidateLayout, true, frame);
}

bool InspectorTimelineAgent::willLayout(Frame* frame)
{
    RenderObject* root = frame->view()->layoutRoot();
    bool partialLayout = !!root;

    if (!partialLayout)
        root = frame->contentRenderer();

    unsigned dirtyObjects = 0;
    unsigned totalObjects = 0;
    for (RenderObject* o = root; o; o = o->nextInPreOrder(root)) {
        ++totalObjects;
        if (o->needsLayout())
            ++dirtyObjects;
    }
    pushCurrentRecord(TimelineRecordFactory::createLayoutData(dirtyObjects, totalObjects, partialLayout), TimelineRecordType::Layout, true, frame);
    return true;
}

void InspectorTimelineAgent::didLayout(RenderObject* root)
{
    if (m_recordStack.isEmpty())
        return;
    TimelineRecordEntry& entry = m_recordStack.last();
    ASSERT(entry.type == TimelineRecordType::Layout);
    Vector<FloatQuad> quads;
    root->absoluteQuads(quads);
    if (quads.size() >= 1)
        TimelineRecordFactory::appendLayoutRoot(entry.data.get(), quads[0]);
    else
        ASSERT_NOT_REACHED();
    didCompleteCurrentRecord(TimelineRecordType::Layout);
}

void InspectorTimelineAgent::didScheduleStyleRecalculation(Document* document)
{
    appendRecord(InspectorObject::create(), TimelineRecordType::ScheduleStyleRecalculation, true, document->frame());
}

bool InspectorTimelineAgent::willRecalculateStyle(Document* document)
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::RecalculateStyles, true, document->frame());
    ASSERT(!m_styleRecalcElementCounter);
    return true;
}

void InspectorTimelineAgent::didRecalculateStyle()
{
    if (m_recordStack.isEmpty())
        return;
    TimelineRecordEntry& entry = m_recordStack.last();
    ASSERT(entry.type == TimelineRecordType::RecalculateStyles);
    TimelineRecordFactory::appendStyleRecalcDetails(entry.data.get(), m_styleRecalcElementCounter);
    m_styleRecalcElementCounter = 0;
    didCompleteCurrentRecord(TimelineRecordType::RecalculateStyles);
}

void InspectorTimelineAgent::didRecalculateStyleForElement()
{
    ++m_styleRecalcElementCounter;
}

void InspectorTimelineAgent::willPaint(Frame* frame)
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::Paint, true, frame, true);
}

void InspectorTimelineAgent::didPaint(RenderObject* renderer, GraphicsContext*, const LayoutRect& clipRect)
{
    TimelineRecordEntry& entry = m_recordStack.last();
    ASSERT(entry.type == TimelineRecordType::Paint);
    FloatQuad quad;
    localToPageQuad(*renderer, clipRect, &quad);
    entry.data = TimelineRecordFactory::createPaintData(quad);
    didCompleteCurrentRecord(TimelineRecordType::Paint);
}

void InspectorTimelineAgent::willScrollLayer(Frame* frame)
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::ScrollLayer, false, frame);
}

void InspectorTimelineAgent::didScrollLayer()
{
    didCompleteCurrentRecord(TimelineRecordType::ScrollLayer);
}

void InspectorTimelineAgent::willDecodeImage(const String& imageType)
{
    pushCurrentRecord(TimelineRecordFactory::createDecodeImageData(imageType), TimelineRecordType::DecodeImage, true, 0);
}

void InspectorTimelineAgent::didDecodeImage()
{
    didCompleteCurrentRecord(TimelineRecordType::DecodeImage);
}

void InspectorTimelineAgent::willResizeImage(bool shouldCache)
{
    pushCurrentRecord(TimelineRecordFactory::createResizeImageData(shouldCache), TimelineRecordType::ResizeImage, true, 0);
}

void InspectorTimelineAgent::didResizeImage()
{
    didCompleteCurrentRecord(TimelineRecordType::ResizeImage);
}

void InspectorTimelineAgent::willComposite()
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::CompositeLayers, false, 0);
}

void InspectorTimelineAgent::didComposite()
{
    didCompleteCurrentRecord(TimelineRecordType::CompositeLayers);
}

bool InspectorTimelineAgent::willWriteHTML(Document* document, unsigned startLine)
{
    pushCurrentRecord(TimelineRecordFactory::createParseHTMLData(startLine), TimelineRecordType::ParseHTML, true, document->frame());
    return true;
}

void InspectorTimelineAgent::didWriteHTML(unsigned endLine)
{
    if (!m_recordStack.isEmpty()) {
        TimelineRecordEntry entry = m_recordStack.last();
        entry.data->setNumber("endLine", endLine);
        didCompleteCurrentRecord(TimelineRecordType::ParseHTML);
    }
}

void InspectorTimelineAgent::didInstallTimer(ScriptExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    appendRecord(TimelineRecordFactory::createTimerInstallData(timerId, timeout, singleShot), TimelineRecordType::TimerInstall, true, frameForScriptExecutionContext(context));
}

void InspectorTimelineAgent::didRemoveTimer(ScriptExecutionContext* context, int timerId)
{
    appendRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerRemove, true, frameForScriptExecutionContext(context));
}

bool InspectorTimelineAgent::willFireTimer(ScriptExecutionContext* context, int timerId)
{
    pushCurrentRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerFire, false, frameForScriptExecutionContext(context));
    return true;
}

void InspectorTimelineAgent::didFireTimer()
{
    didCompleteCurrentRecord(TimelineRecordType::TimerFire);
}

bool InspectorTimelineAgent::willDispatchXHRReadyStateChangeEvent(ScriptExecutionContext* context, XMLHttpRequest* request)
{
    if (!request->hasEventListeners(eventNames().readystatechangeEvent))
        return false;
    pushCurrentRecord(TimelineRecordFactory::createXHRReadyStateChangeData(request->url().string(), request->readyState()), TimelineRecordType::XHRReadyStateChange, false, frameForScriptExecutionContext(context));
    return true;
}

void InspectorTimelineAgent::didDispatchXHRReadyStateChangeEvent()
{
    didCompleteCurrentRecord(TimelineRecordType::XHRReadyStateChange);
}

bool InspectorTimelineAgent::willDispatchXHRLoadEvent(ScriptExecutionContext* context, XMLHttpRequest* request)
{
    if (!request->hasEventListeners(eventNames().loadEvent))
        return false;
    pushCurrentRecord(TimelineRecordFactory::createXHRLoadData(request->url()), TimelineRecordType::XHRLoad, true, frameForScriptExecutionContext(context));
    return true;
}

void InspectorTimelineAgent::didDispatchXHRLoadEvent()
{
    didCompleteCurrentRecord(TimelineRecordType::XHRLoad);
}

bool InspectorTimelineAgent::willEvaluateScript(Frame* frame, const String& url, int lineNumber)
{
    pushCurrentRecord(TimelineRecordFactory::createEvaluateScriptData(url, lineNumber), TimelineRecordType::EvaluateScript, true, frame);
    return true;
}

void InspectorTimelineAgent::didEvaluateScript()
{
    didCompleteCurrentRecord(TimelineRecordType::EvaluateScript);
}

void InspectorTimelineAgent::didScheduleResourceRequest(Document* document, const String& url)
{
    appendRecord(TimelineRecordFactory::createScheduleResourceRequestData(url), TimelineRecordType::ScheduleResourceRequest, true, document->frame());
}

void InspectorTimelineAgent::willSendResourceRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request, const ResourceResponse&)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    appendRecord(TimelineRecordFactory::createResourceSendRequestData(requestId, request), TimelineRecordType::ResourceSendRequest, true, loader->frame());
}

bool InspectorTimelineAgent::willReceiveResourceData(Frame* frame, unsigned long identifier, int length)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    pushCurrentRecord(TimelineRecordFactory::createReceiveResourceData(requestId, length), TimelineRecordType::ResourceReceivedData, false, frame);
    return true;
}

void InspectorTimelineAgent::didReceiveResourceData()
{
    didCompleteCurrentRecord(TimelineRecordType::ResourceReceivedData);
}

bool InspectorTimelineAgent::willReceiveResourceResponse(Frame* frame, unsigned long identifier, const ResourceResponse& response)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    pushCurrentRecord(TimelineRecordFactory::createResourceReceiveResponseData(requestId, response), TimelineRecordType::ResourceReceiveResponse, false, frame);
    return true;
}

void InspectorTimelineAgent::didReceiveResourceResponse(unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    didCompleteCurrentRecord(TimelineRecordType::ResourceReceiveResponse);
}

void InspectorTimelineAgent::didFinishLoadingResource(unsigned long identifier, bool didFail, double finishTime, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createResourceFinishData(IdentifiersFactory::requestId(identifier), didFail, finishTime * 1000), TimelineRecordType::ResourceFinish, false, frame);
}

void InspectorTimelineAgent::didFailLoading(unsigned long identifier, DocumentLoader* loader, const ResourceError& error)
{
    didFinishLoadingResource(identifier, true, 0, loader->frame());
}

void InspectorTimelineAgent::didTimeStamp(Frame* frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeStamp, true, frame);
}

void InspectorTimelineAgent::time(Frame* frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::Time, true, frame);
}

void InspectorTimelineAgent::timeEnd(Frame* frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeEnd, true, frame);
}

void InspectorTimelineAgent::didMarkDOMContentEvent(Frame* frame)
{
    bool isMainFrame = frame && m_pageAgent && (frame == m_pageAgent->mainFrame());
    appendRecord(TimelineRecordFactory::createMarkData(isMainFrame), TimelineRecordType::MarkDOMContent, false, frame);
}

void InspectorTimelineAgent::didMarkLoadEvent(Frame* frame)
{
    bool isMainFrame = frame && m_pageAgent && (frame == m_pageAgent->mainFrame());
    appendRecord(TimelineRecordFactory::createMarkData(isMainFrame), TimelineRecordType::MarkLoad, false, frame);
}

void InspectorTimelineAgent::didCommitLoad()
{
    clearRecordStack();
}

void InspectorTimelineAgent::didRequestAnimationFrame(Document* document, int callbackId)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::RequestAnimationFrame, true, document->frame());
}

void InspectorTimelineAgent::didCancelAnimationFrame(Document* document, int callbackId)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::CancelAnimationFrame, true, document->frame());
}

bool InspectorTimelineAgent::willFireAnimationFrame(Document* document, int callbackId)
{
    pushCurrentRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::FireAnimationFrame, false, document->frame());
    return true;
}

void InspectorTimelineAgent::didFireAnimationFrame()
{
    didCompleteCurrentRecord(TimelineRecordType::FireAnimationFrame);
}

void InspectorTimelineAgent::willProcessTask()
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::Program, false, 0);
}

void InspectorTimelineAgent::didProcessTask()
{
    didCompleteCurrentRecord(TimelineRecordType::Program);
}

void InspectorTimelineAgent::didCreateWebSocket(Document* document, unsigned long identifier, const KURL& url, const String& protocol)
{
    appendRecord(TimelineRecordFactory::createWebSocketCreateData(identifier, url, protocol), TimelineRecordType::WebSocketCreate, true, document->frame());
}

void InspectorTimelineAgent::willSendWebSocketHandshakeRequest(Document* document, unsigned long identifier, const WebSocketHandshakeRequest&)
{
    appendRecord(TimelineRecordFactory::createGenericWebSocketData(identifier), TimelineRecordType::WebSocketSendHandshakeRequest, true, document->frame());
}

void InspectorTimelineAgent::didReceiveWebSocketHandshakeResponse(Document* document, unsigned long identifier, const WebSocketHandshakeResponse&)
{
    appendRecord(TimelineRecordFactory::createGenericWebSocketData(identifier), TimelineRecordType::WebSocketReceiveHandshakeResponse, false, document->frame());
}

void InspectorTimelineAgent::didCloseWebSocket(Document* document, unsigned long identifier)
{
    appendRecord(TimelineRecordFactory::createGenericWebSocketData(identifier), TimelineRecordType::WebSocketDestroy, true, document->frame());
}

void InspectorTimelineAgent::addRecordToTimeline(PassRefPtr<InspectorObject> record, const String& type)
{
    commitFrameRecord();
    innerAddRecordToTimeline(record, type);
}

void InspectorTimelineAgent::innerAddRecordToTimeline(PassRefPtr<InspectorObject> prpRecord, const String& type)
{
    prpRecord->setString("type", type);
    RefPtr<TypeBuilder::Timeline::TimelineEvent> record = TypeBuilder::Timeline::TimelineEvent::runtimeCast(prpRecord);
    if (type == TimelineRecordType::Program)
        setNativeHeapStatistics(record.get());
    else
        setDOMCounters(record.get());

    if (m_recordStack.isEmpty())
        sendEvent(record.release());
    else {
        TimelineRecordEntry parent = m_recordStack.last();
        parent.children->pushObject(record.release());
    }
}

static size_t getUsedHeapSize()
{
    HeapInfo info;
    ScriptGCEvent::getHeapSize(info);
    return info.usedJSHeapSize;
}

void InspectorTimelineAgent::setDOMCounters(TypeBuilder::Timeline::TimelineEvent* record)
{
    record->setUsedHeapSize(getUsedHeapSize());

    if (m_state->getBoolean(TimelineAgentState::includeDomCounters)) {
        int documentCount = 0;
        int nodeCount = 0;
        if (m_inspectorType == PageInspector) {
            documentCount = InspectorCounters::counterValue(InspectorCounters::DocumentCounter);
            nodeCount = InspectorCounters::counterValue(InspectorCounters::NodeCounter);
        }
        int listenerCount = ThreadLocalInspectorCounters::current().counterValue(ThreadLocalInspectorCounters::JSEventListenerCounter);
        RefPtr<TypeBuilder::Timeline::DOMCounters> counters = TypeBuilder::Timeline::DOMCounters::create()
            .setDocuments(documentCount)
            .setNodes(nodeCount)
            .setJsEventListeners(listenerCount);
        record->setCounters(counters.release());
    }
}

void InspectorTimelineAgent::setNativeHeapStatistics(TypeBuilder::Timeline::TimelineEvent* record)
{
    if (!m_memoryAgent)
        return;
    if (!m_state->getBoolean(TimelineAgentState::includeNativeMemoryStatistics))
        return;
    HashMap<String, size_t> map;
    m_memoryAgent->getProcessMemoryDistributionMap(&map);
    RefPtr<InspectorObject> stats = InspectorObject::create();
    for (HashMap<String, size_t>::iterator it = map.begin(); it != map.end(); ++it)
        stats->setNumber(it->key, it->value);
    size_t privateBytes = 0;
    size_t sharedBytes = 0;
    MemoryUsageSupport::processMemorySizesInBytes(&privateBytes, &sharedBytes);
    stats->setNumber("PrivateBytes", privateBytes);
    record->setNativeHeapStatistics(stats.release());
}

void InspectorTimelineAgent::setFrameIdentifier(InspectorObject* record, Frame* frame)
{
    if (!frame || !m_pageAgent)
        return;
    String frameId;
    if (frame && m_pageAgent)
        frameId = m_pageAgent->frameId(frame);
    record->setString("frameId", frameId);
}

void InspectorTimelineAgent::didCompleteCurrentRecord(const String& type)
{
    // An empty stack could merely mean that the timeline agent was turned on in the middle of
    // an event.  Don't treat as an error.
    if (!m_recordStack.isEmpty()) {
        if (m_platformInstrumentationClientInstalledAtStackDepth == m_recordStack.size()) {
            m_platformInstrumentationClientInstalledAtStackDepth = 0;
            PlatformInstrumentation::setClient(0);
        }

        pushGCEventRecords();
        TimelineRecordEntry entry = m_recordStack.last();
        m_recordStack.removeLast();
        ASSERT(entry.type == type);
        entry.record->setObject("data", entry.data);
        entry.record->setArray("children", entry.children);
        entry.record->setNumber("endTime", timestamp());
        size_t usedHeapSizeDelta = getUsedHeapSize() - entry.usedHeapSizeAtStart;
        if (usedHeapSizeDelta)
            entry.record->setNumber("usedHeapSizeDelta", usedHeapSizeDelta);
        addRecordToTimeline(entry.record, type);
    }
}

InspectorTimelineAgent::InspectorTimelineAgent(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InspectorMemoryAgent* memoryAgent, InspectorCompositeState* state, InspectorType type, InspectorClient* client)
    : InspectorBaseAgent<InspectorTimelineAgent>("Timeline", instrumentingAgents, state)
    , m_pageAgent(pageAgent)
    , m_memoryAgent(memoryAgent)
    , m_frontend(0)
    , m_id(1)
    , m_maxCallStackDepth(5)
    , m_platformInstrumentationClientInstalledAtStackDepth(0)
    , m_inspectorType(type)
    , m_client(client)
    , m_weakFactory(this)
    , m_styleRecalcElementCounter(0)
{
}

void InspectorTimelineAgent::appendRecord(PassRefPtr<InspectorObject> data, const String& type, bool captureCallStack, Frame* frame)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0);
    record->setObject("data", data);
    setFrameIdentifier(record.get(), frame);
    addRecordToTimeline(record.release(), type);
}

void InspectorTimelineAgent::sendEvent(PassRefPtr<InspectorObject> event)
{
    // FIXME: runtimeCast is a hack. We do it because we can't build TimelineEvent directly now.
    RefPtr<TypeBuilder::Timeline::TimelineEvent> recordChecked = TypeBuilder::Timeline::TimelineEvent::runtimeCast(event);
    m_frontend->eventRecorded(recordChecked.release());
}

void InspectorTimelineAgent::pushCurrentRecord(PassRefPtr<InspectorObject> data, const String& type, bool captureCallStack, Frame* frame, bool hasLowLevelDetails)
{
    pushGCEventRecords();
    commitFrameRecord();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0);
    setFrameIdentifier(record.get(), frame);
    m_recordStack.append(TimelineRecordEntry(record.release(), data, InspectorArray::create(), type, getUsedHeapSize()));
    if (hasLowLevelDetails && !m_platformInstrumentationClientInstalledAtStackDepth && !PlatformInstrumentation::hasClient()) {
        m_platformInstrumentationClientInstalledAtStackDepth = m_recordStack.size();
        PlatformInstrumentation::setClient(this);
    }
}

void InspectorTimelineAgent::commitFrameRecord()
{
    if (!m_pendingFrameRecord)
        return;
    
    m_pendingFrameRecord->setObject("data", InspectorObject::create());
    innerAddRecordToTimeline(m_pendingFrameRecord.release(), TimelineRecordType::BeginFrame);
}

void InspectorTimelineAgent::clearRecordStack()
{
    if (m_platformInstrumentationClientInstalledAtStackDepth) {
        m_platformInstrumentationClientInstalledAtStackDepth = 0;
        PlatformInstrumentation::setClient(0);
    }
    m_pendingFrameRecord.clear();
    m_recordStack.clear();
    m_id++;
}

void InspectorTimelineAgent::localToPageQuad(const RenderObject& renderer, const LayoutRect& rect, FloatQuad* quad)
{
    Frame* frame = renderer.frame();
    FrameView* view = frame->view();
    FloatQuad absolute = renderer.localToAbsoluteQuad(FloatQuad(rect));
    quad->setP1(view->contentsToRootView(roundedIntPoint(absolute.p1())));
    quad->setP2(view->contentsToRootView(roundedIntPoint(absolute.p2())));
    quad->setP3(view->contentsToRootView(roundedIntPoint(absolute.p3())));
    quad->setP4(view->contentsToRootView(roundedIntPoint(absolute.p4())));
}

double InspectorTimelineAgent::timestamp()
{
    return m_timeConverter.fromMonotonicallyIncreasingTime(WTF::monotonicallyIncreasingTime());
}

Page* InspectorTimelineAgent::page()
{
    return m_pageAgent ? m_pageAgent->page() : 0;
}

} // namespace WebCore

