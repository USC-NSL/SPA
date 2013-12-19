/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/V8Binding.h"

#include "V8DOMStringList.h"
#include "V8DOMWindow.h"
#include "V8Element.h"
#include "V8WorkerContext.h"
#include "V8XPathNSResolver.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8NodeFilterCondition.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/WorkerScriptController.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Element.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/inspector/BindingVisitors.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Frame.h"
#include "core/page/Settings.h"
#include "core/workers/WorkerContext.h"
#include "core/xml/XPathNSResolver.h"
#include "wtf/MainThread.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Threading.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

v8::Handle<v8::Value> setDOMException(int exceptionCode, v8::Isolate* isolate)
{
    return V8ThrowException::setDOMException(exceptionCode, isolate);
}

v8::Handle<v8::Value> throwError(V8ErrorType errorType, const char* message, v8::Isolate* isolate)
{
    return V8ThrowException::throwError(errorType, message, isolate);
}

v8::Handle<v8::Value> throwError(v8::Local<v8::Value> exception, v8::Isolate* isolate)
{
    return V8ThrowException::throwError(exception, isolate);
}

v8::Handle<v8::Value> throwTypeError(const char* message, v8::Isolate* isolate)
{
    return V8ThrowException::throwTypeError(message, isolate);
}

v8::Handle<v8::Value> throwNotEnoughArgumentsError(v8::Isolate* isolate)
{
    return V8ThrowException::throwNotEnoughArgumentsError(isolate);
}

v8::Handle<v8::Value> v8Array(PassRefPtr<DOMStringList> stringList, v8::Isolate* isolate)
{
    if (!stringList)
        return v8::Array::New();
    v8::Local<v8::Array> result = v8::Array::New(stringList->length());
    for (unsigned i = 0; i < stringList->length(); ++i)
        result->Set(v8Integer(i, isolate), v8String(stringList->item(i), isolate));
    return result;
}

Vector<v8::Handle<v8::Value> > toVectorOfArguments(const v8::Arguments& args)
{
    Vector<v8::Handle<v8::Value> > result;
    size_t length = args.Length();
    for (size_t i = 0; i < length; ++i)
        result.append(args[i]);
    return result;
}

PassRefPtr<NodeFilter> toNodeFilter(v8::Handle<v8::Value> callback)
{
    return NodeFilter::create(V8NodeFilterCondition::create(callback));
}

const int32_t kMaxInt32 = 0x7fffffff;
const int32_t kMinInt32 = -kMaxInt32 - 1;
const uint32_t kMaxUInt32 = 0xffffffff;
const int64_t kJSMaxInteger = 0x20000000000000LL - 1; // 2^53 - 1, maximum integer exactly representable in ECMAScript.

static double enforceRange(double x, double minimum, double maximum, bool& ok)
{
    if (std::isnan(x) || std::isinf(x)) {
        ok = false;
        return 0;
    }
    x = trunc(x);
    if (x < minimum || x > maximum) {
        ok = false;
        return 0;
    }
    return x;
}

int32_t toInt32(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    ok = true;

    // Fast case. The value is already a 32-bit integer.
    if (value->IsInt32())
        return value->Int32Value();

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    if (configuration == EnforceRange)
        return enforceRange(numberObject->Value(), kMinInt32, kMaxInt32, ok);

    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (std::isnan(numberValue) || std::isinf(numberValue))
        return 0;
    return numberObject->Int32Value();
}

uint32_t toUInt32(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    ok = true;

    // Fast case. The value is already a 32-bit unsigned integer.
    if (value->IsUint32())
        return value->Uint32Value();

    // Fast case. The value is a 32-bit signed integer - possibly positive?
    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= 0)
            return result;
        if (configuration == EnforceRange) {
            ok = false;
            return 0;
        }
        return result;
    }

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    if (configuration == EnforceRange)
        return enforceRange(numberObject->Value(), 0, kMaxUInt32, ok);

    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (std::isnan(numberValue) || std::isinf(numberValue))
        return 0;
    return numberObject->Uint32Value();
}

int64_t toInt64(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    ok = true;

    // Fast case. The value is a 32-bit integer.
    if (value->IsInt32())
        return value->Int32Value();

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    double x = numberObject->Value();

    if (configuration == EnforceRange)
        return enforceRange(x, -kJSMaxInteger, kJSMaxInteger, ok);

    // NaNs and +/-Infinity should be 0, otherwise modulo 2^64.
    unsigned long long integer;
    doubleToInteger(x, integer);
    return integer;
}

uint64_t toUInt64(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, bool& ok)
{
    ok = true;

    // Fast case. The value is a 32-bit unsigned integer.
    if (value->IsUint32())
        return value->Uint32Value();

    // Fast case. The value is a 32-bit integer.
    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= 0)
            return result;
        if (configuration == EnforceRange) {
            ok = false;
            return 0;
        }
        return result;
    }

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    double x = numberObject->Value();

    if (configuration == EnforceRange)
        return enforceRange(x, 0, kJSMaxInteger, ok);

    // NaNs and +/-Infinity should be 0, otherwise modulo 2^64.
    unsigned long long integer;
    doubleToInteger(x, integer);
    return integer;
}

v8::Persistent<v8::FunctionTemplate> createRawTemplate(v8::Isolate* isolate)
{
    v8::HandleScope scope;
    v8::Local<v8::FunctionTemplate> result = v8::FunctionTemplate::New(V8ObjectConstructor::isValidConstructorMode);
    return v8::Persistent<v8::FunctionTemplate>::New(isolate, result);
}        

PassRefPtr<DOMStringList> toDOMStringList(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(value));

    if (V8DOMStringList::HasInstance(v8Value, isolate, worldType(isolate))) {
        RefPtr<DOMStringList> ret = V8DOMStringList::toNative(v8::Handle<v8::Object>::Cast(v8Value));
        return ret.release();
    }

    if (!v8Value->IsArray())
        return 0;

    RefPtr<DOMStringList> ret = DOMStringList::create();
    v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(v8Value);
    for (size_t i = 0; i < v8Array->Length(); ++i) {
        v8::Local<v8::Value> indexedValue = v8Array->Get(v8Integer(i, isolate));
        ret->append(toWebCoreString(indexedValue));
    }
    return ret.release();
}

PassRefPtr<XPathNSResolver> toXPathNSResolver(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    RefPtr<XPathNSResolver> resolver;
    if (V8XPathNSResolver::HasInstance(value, isolate, worldType(isolate)))
        resolver = V8XPathNSResolver::toNative(v8::Handle<v8::Object>::Cast(value));
    else if (value->IsObject())
        resolver = V8CustomXPathNSResolver::create(value->ToObject(), isolate);
    return resolver;
}

v8::Handle<v8::Object> toInnerGlobalObject(v8::Handle<v8::Context> context)
{
    return v8::Handle<v8::Object>::Cast(context->Global()->GetPrototype());
}

DOMWindow* toDOMWindow(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    v8::Handle<v8::Object> window = global->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate(context->GetIsolate(), MainWorld));
    if (!window.IsEmpty())
        return V8DOMWindow::toNative(window);
    window = global->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate(context->GetIsolate(), IsolatedWorld));
    ASSERT(!window.IsEmpty());
    return V8DOMWindow::toNative(window);
}

ScriptExecutionContext* toScriptExecutionContext(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    v8::Handle<v8::Object> windowWrapper = global->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate(context->GetIsolate(), MainWorld));
    if (!windowWrapper.IsEmpty())
        return V8DOMWindow::toNative(windowWrapper)->scriptExecutionContext();
    windowWrapper = global->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate(context->GetIsolate(), IsolatedWorld));
    if (!windowWrapper.IsEmpty())
        return V8DOMWindow::toNative(windowWrapper)->scriptExecutionContext();
    v8::Handle<v8::Object> workerWrapper = global->FindInstanceInPrototypeChain(V8WorkerContext::GetTemplate(context->GetIsolate(), WorkerWorld));
    if (!workerWrapper.IsEmpty())
        return V8WorkerContext::toNative(workerWrapper)->scriptExecutionContext();
    // FIXME: Is this line of code reachable?
    return 0;
}

Frame* toFrameIfNotDetached(v8::Handle<v8::Context> context)
{
    DOMWindow* window = toDOMWindow(context);
    if (window->isCurrentlyDisplayedInFrame())
        return window->frame();
    // We return 0 here because |context| is detached from the Frame. If we
    // did return |frame| we could get in trouble because the frame could be
    // navigated to another security origin.
    return 0;
}

v8::Local<v8::Context> toV8Context(ScriptExecutionContext* context, DOMWrapperWorld* world)
{
    if (context->isDocument()) {
        ASSERT(world);
        if (Frame* frame = toDocument(context)->frame())
            return frame->script()->windowShell(world)->context();
    } else if (context->isWorkerContext()) {
        ASSERT(!world);
        if (WorkerScriptController* script = static_cast<WorkerContext*>(context)->script())
            return script->context();
    }
    return v8::Local<v8::Context>();
}

bool handleOutOfMemory()
{
    v8::Local<v8::Context> context = v8::Context::GetCurrent();

    if (!context->HasOutOfMemoryException())
        return false;

    // Warning, error, disable JS for this frame?
    Frame* frame = toFrameIfNotDetached(context);
    if (!frame)
        return true;

    frame->script()->clearForOutOfMemory();
    frame->loader()->client()->didExhaustMemoryAvailableForScript();

    if (Settings* settings = frame->settings())
        settings->setScriptEnabled(false);

    return true;
}

v8::Local<v8::Value> handleMaxRecursionDepthExceeded()
{
    throwError(v8RangeError, "Maximum call stack size exceeded.", v8::Isolate::GetCurrent());
    return v8::Local<v8::Value>();
}

void crashIfV8IsDead()
{
    if (v8::V8::IsDead()) {
        // FIXME: We temporarily deal with V8 internal error situations
        // such as out-of-memory by crashing the renderer.
        CRASH();
    }
}

WrapperWorldType worldType(v8::Isolate* isolate)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    if (!data->workerDOMDataStore())
        return worldTypeInMainThread(isolate);
    return WorkerWorld;
}

WrapperWorldType worldTypeInMainThread(v8::Isolate* isolate)
{
    if (!DOMWrapperWorld::isolatedWorldsExist())
        return MainWorld;
    ASSERT(!v8::Context::GetEntered().IsEmpty());
    DOMWrapperWorld* isolatedWorld = DOMWrapperWorld::isolatedWorld(v8::Context::GetEntered());
    if (isolatedWorld)
        return IsolatedWorld;
    return MainWorld;
}

} // namespace WebCore
