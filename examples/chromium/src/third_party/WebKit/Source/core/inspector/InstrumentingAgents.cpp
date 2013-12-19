/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "core/inspector/InstrumentingAgents.h"

#include "core/inspector/InspectorController.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/page/Page.h"
#include "core/workers/WorkerContext.h"
#include <wtf/MainThread.h>

namespace WebCore {

InstrumentingAgents::InstrumentingAgents()
    : m_inspectorAgent(0)
    , m_inspectorPageAgent(0)
    , m_inspectorCSSAgent(0)
    , m_inspectorLayerTreeAgent(0)
    , m_inspectorConsoleAgent(0)
    , m_inspectorDOMAgent(0)
    , m_inspectorResourceAgent(0)
    , m_pageRuntimeAgent(0)
    , m_workerRuntimeAgent(0)
    , m_inspectorTimelineAgent(0)
    , m_inspectorDOMStorageAgent(0)
    , m_inspectorDatabaseAgent(0)
    , m_inspectorFileSystemAgent(0)
    , m_inspectorApplicationCacheAgent(0)
    , m_inspectorDebuggerAgent(0)
    , m_pageDebuggerAgent(0)
    , m_inspectorDOMDebuggerAgent(0)
    , m_inspectorProfilerAgent(0)
    , m_inspectorWorkerAgent(0)
    , m_inspectorCanvasAgent(0)
{
}

void InstrumentingAgents::reset()
{
    m_inspectorAgent = 0;
    m_inspectorPageAgent = 0;
    m_inspectorCSSAgent = 0;
    m_inspectorLayerTreeAgent = 0;
    m_inspectorConsoleAgent = 0;
    m_inspectorDOMAgent = 0;
    m_inspectorResourceAgent = 0;
    m_pageRuntimeAgent = 0;
    m_workerRuntimeAgent = 0;
    m_inspectorTimelineAgent = 0;
    m_inspectorDOMStorageAgent = 0;
    m_inspectorDatabaseAgent = 0;
    m_inspectorFileSystemAgent = 0;
    m_inspectorApplicationCacheAgent = 0;
    m_inspectorDebuggerAgent = 0;
    m_pageDebuggerAgent = 0;
    m_inspectorDOMDebuggerAgent = 0;
    m_inspectorProfilerAgent = 0;
    m_inspectorWorkerAgent = 0;
    m_inspectorCanvasAgent = 0;
}

InstrumentingAgents* instrumentationForPage(Page* page)
{
    ASSERT(isMainThread());
    if (InspectorController* controller = page->inspectorController())
        return controller->m_instrumentingAgents.get();
    return 0;
}

InstrumentingAgents* instrumentationForWorkerContext(WorkerContext* workerContext)
{
    if (WorkerInspectorController* controller = workerContext->workerInspectorController())
        return controller->m_instrumentingAgents.get();
    return 0;
}

} // namespace WebCore
