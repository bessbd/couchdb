// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef XP_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <jsapi.h>
#include <js/Initialization.h>

#include "config.h"
#include "http.h"
#include "utf8.h"
#include "util.h"



static JSClassOps global_ops = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    JS_GlobalObjectTraceHook
};

/* The class of the global object. */
static JSClass global_class = {
    "global",
    JSCLASS_GLOBAL_FLAGS,
    &global_ops
};


static void
req_dtor(JSFreeOp* fop, JSObject* obj)
{
    http_dtor(fop, obj);
}

// With JSClass.construct.
static const JSClassOps clsOps = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    req_dtor,
    nullptr,
    nullptr,
    nullptr
};

static const JSClass CouchHTTPClass = {
    "CouchHTTP",  /* name */
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(2),        /* flags */
    &clsOps
};

static bool
req_ctor(JSContext* cx, unsigned int argc, JS::Value* vp)
{
    bool ret;
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JSObject* obj = JS_NewObjectForConstructor(cx, &CouchHTTPClass, args);
    if(!obj) {
        JS_ReportErrorUTF8(cx, "Failed to create CouchHTTP instance.\n", NULL);
        return false;
    }
    ret = http_ctor(cx, obj);
    args.rval().setObject(*obj);
    return ret;
}

static bool
req_open(JSContext* cx, unsigned int argc, JS::Value* vp)
{
    JSObject* obj = JS_THIS_OBJECT(cx, vp);
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    bool ret = false;

    if(argc == 2) {
        ret = http_open(cx, obj, args[0], args[1], JS::BooleanValue(false));
    } else if(argc == 3) {
        ret = http_open(cx, obj, args[0], args[1], args[2]);
    } else {
        JS_ReportErrorUTF8(cx, "Invalid call to CouchHTTP.open");
    }

    args.rval().setUndefined();
    return ret;
}


static bool
req_set_hdr(JSContext* cx, unsigned int argc, JS::Value* vp)
{
    JSObject* obj = JS_THIS_OBJECT(cx, vp);
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    bool ret = false;

    if(argc == 2) {
        ret = http_set_hdr(cx, obj, args[0], args[1]);
    } else {
        JS_ReportErrorUTF8(cx, "Invalid call to CouchHTTP.set_header");
    }

    args.rval().setUndefined();
    return ret;
}


static bool
req_send(JSContext* cx, unsigned int argc, JS::Value* vp)
{
    JSObject* obj = JS_THIS_OBJECT(cx, vp);
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    bool ret = false;

    if(argc == 1) {
        ret = http_send(cx, obj, args[0]);
    } else {
        JS_ReportErrorUTF8(cx, "Invalid call to CouchHTTP.send");
    }

    args.rval().setUndefined();
    return ret;
}

// TBD JSPropertySpec related change
static bool
req_status(JSContext* cx, unsigned int argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(0, vp);
    //int status = http_status(cx, obj);
    int status;

    if(status < 0)
        return false;

    args.rval().set(JS::Int32Value(status));
    return true;
}

// TBD JSPropertySpec related change
static bool
//base_url(JSContext *cx, JSObject* obj, jsid pid, JS::Value* vp)
base_url(JSContext *cx, unsigned int argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(0, vp);
    return true;

    //couch_args *args = (couch_args*)JS_GetContextPrivate(cx);
    //return http_uri(cx, obj, args, &JS_RVAL(cx, vp));
}


static bool
evalcx(JSContext *cx, unsigned int argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    JSString* str;
    //JSObject* sandbox;
    JSObject* global;
    JSContext* subcx;
    JSCompartment* call = NULL;
    const char16_t* src;
    size_t srclen;
    JS::Value rval;
    bool ret = false;
    char *name = NULL;
    JS::RootedObject sandbox(cx);

    sandbox = NULL;
    //if(!JS_ConvertArguments(cx, argc, args, "S / o", &str, &sandbox)) {
    //    return false;
    //}

    subcx = JS_NewContext(8L * 1024L, 8L * 1024L, JS_GetRuntime(cx));
    if(!subcx) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    JS_BeginRequest(subcx);
    JSAutoRequest ar(subcx);

    src = (const char16_t *)js_to_string(cx, str).c_str();
    srclen = JS_GetStringLength(str);

    // Re-use the compartment associated with the main context,
    // rather than creating a new compartment */
    // TBD find way to get existing global object
    //global = JS_GetGlobalObject(cx);
    if(global == NULL) goto done;
    call = JS_EnterCompartment(subcx, global);

    if(!sandbox) {
        //sandbox = JS_NewGlobalObject(subcx, &global_class);
        JS::CompartmentOptions options;
    #ifdef ENABLE_STREAMS
        options.creationOptions().setStreamsEnabled(true);
    #endif
        sandbox = JS_NewGlobalObject(subcx,
            &global_class,
            nullptr,
            JS::FireOnNewGlobalHook,
            options);
        if(!sandbox || !JS_InitStandardClasses(subcx, sandbox)) {
            goto done;
        }
    }

    if(argc > 2) {
        name = enc_string(cx, args[2], NULL);
    }

    if(srclen == 0) {
        args.rval().setUndefined();  //TBC
    } else {
        JS::CompileOptions opts(cx);
        JS::RootedValue rval(cx);
        opts.setFileAndLine(__FILE__, __LINE__);
        JS::Evaluate(subcx, opts, src, srclen, &rval);
        args.rval().set(rval);
    }
    
    ret = true;

done:
    if(name) JS_free(cx, name);
    JS_LeaveCompartment(cx, call);
    JS_DestroyContext(subcx);
    return ret;
}


static bool
gc(JSContext* cx, unsigned int argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS_GC(cx);
    args.rval().setUndefined();
    return true;
}


static bool
print(JSContext* cx, unsigned int argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    couch_print(cx, argc, args);
    args.rval().setUndefined();
    return true;
}


static bool
quit(JSContext* cx, unsigned int argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    int exit_code = 0;
    //JS_ConvertArguments(cx, argc, args, "/i", &exit_code);
    exit(exit_code);
}


static bool
readline(JSContext* cx, unsigned int argc, JS::Value* vp)
{
    JSString* line;
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    /* GC Occasionally */
    JS_MaybeGC(cx);

    line = couch_readline(cx, stdin);
    if(line == NULL) return false;

    args.rval().setString(line);
    return true;
}


static bool
seal(JSContext* cx, unsigned int argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JSObject *target;
    bool deep = false;
    bool ret;

    //if(!JS_ConvertArguments(cx, argc, argv, "o/b", &target, &deep))
    //    return false;

    if(!target) {
        args.rval().setUndefined();
        return true;
    }

    JS::RootedObject rtarget(cx, target);
    ret = deep ? JS_DeepFreezeObject(cx, rtarget) : JS_FreezeObject(cx, rtarget);
    args.rval().setUndefined();
    return ret;
}


static bool
js_sleep(JSContext* cx, unsigned int argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    int duration = 0;
    //if(!JS_ConvertArguments(cx, argc, argv, "/i", &duration)) {
    //    return false;
    //}

#ifdef XP_WIN
    Sleep(duration);
#else
    usleep(duration * 1000);
#endif

    return true;
}

JSPropertySpec CouchHTTPProperties[] = {
    //{"status", 0, JSPROP_READONLY, req_status, NULL},
    //{"base_url", 0, JSPROP_READONLY | JSPROP_SHARED, base_url, NULL},
    //{0, 0, 0, 0, 0}
    JS_PSG("status", req_status, 0),
    JS_PSG("base_url", base_url, 0),
    JS_PS_END
};


JSFunctionSpec CouchHTTPFunctions[] = {
    JS_FN("_open", req_open, 3, 0),
    JS_FN("_setRequestHeader", req_set_hdr, 2, 0),
    JS_FN("_send", req_send, 1, 0),
    JS_FS_END
};


JSFunctionSpec TestSuiteFunctions[] = {
    JS_FN("sleep", js_sleep, 1, 0),
    JS_FS_END
};


static JSFunctionSpec global_functions[] = {
    JS_FN("evalcx", evalcx, 0, 0),
    JS_FN("gc", gc, 0, 0),
    JS_FN("print", print, 0, 0),
    JS_FN("quit", quit, 0, 0),
    JS_FN("readline", readline, 0, 0),
    JS_FN("seal", seal, 0, 0),
    JS_FS_END
};


static bool
csp_allows(JSContext* cx)
{
    couch_args *args = (couch_args*)JS_GetContextPrivate(cx);
    if(args->eval) {
        return true;
    } else {
        return false;
    }
}


static JSSecurityCallbacks security_callbacks = {
    csp_allows,
    nullptr
};


int
main(int argc, const char* argv[])
{
    JSRuntime* rt = NULL;
    JSContext* cx = NULL;
    JSObject* klass = NULL;
    JSSCRIPT_TYPE script;
    JSString* scriptsrc;
    const char* schars;
    size_t slen;
    int i;

    couch_args* args = couch_parse_args(argc, argv);

    if(rt == NULL)
        return 1;

    JS_Init();
    cx = JS_NewContext(args->stack_size, 8L * 1024L);
    if(cx == NULL)
        return 1;

    JS::SetWarningReporter(cx, couch_error);
    //JS_ToggleOptions(cx, JSOPTION_XML);  //TBC
    //JS_SetOptions(cx, JSOPTION_METHODJIT); //TBC


#ifdef JSOPTION_TYPE_INFERENCE
    JS_SetOptions(cx, JSOPTION_TYPE_INFERENCE);
#endif
    JS_SetContextPrivate(cx, args);
    JS_SetSecurityCallbacks(cx, &security_callbacks);

    JSAutoRequest ar(cx);

    JS::RootedObject global(cx);

    JS::CompartmentOptions options;
#ifdef ENABLE_STREAMS
    options.creationOptions().setStreamsEnabled(true);
#endif
    global = JS_NewGlobalObject(cx,
        &global_class,
        nullptr,
        JS::FireOnNewGlobalHook,
        options);

    if (!global)
        return 1;
    JSAutoCompartment ac(cx, global);


    if(!JS_InitStandardClasses(cx, global))
        return 1;

    if(couch_load_funcs(cx, global, global_functions) != true)
        return 1;
 
    if(args->use_http) {
        http_check_enabled();

        klass = JS_InitClass(
            cx, global,
            NULL,
            &CouchHTTPClass, req_ctor,
            0,
            CouchHTTPProperties, CouchHTTPFunctions,
            NULL, NULL
        );

        if(!klass)
        {
            fprintf(stderr, "Failed to initialize CouchHTTP class.\n");
            exit(2);
        }
    } 

    if(args->use_test_funs) {
        if(couch_load_funcs(cx, global, TestSuiteFunctions) != true)
            return 1;
    }

    for(i = 0 ; args->scripts[i] ; i++) {
        // Convert script source to jschars.
        scriptsrc = couch_readfile(cx, args->scripts[i]);
        if(!scriptsrc)
            return 1;

        schars = js_to_string(cx, scriptsrc).c_str();
        slen = JS_GetStringLength(scriptsrc);

        // Root it so GC doesn't collect it. <- TBC
        JS::PersistentRootedString sroot(cx, scriptsrc);

        // Compile and run
        JS::CompileOptions options(cx);
        options.setFileAndLine(__FILE__, __LINE__);
        JS::RootedScript script(cx);
        if(!JS_CompileScript(cx, schars, slen, options, &script)) {
            fprintf(stderr, "Failed to compile script.\n");
            return 1;
        }

        JS::RootedValue result(cx);
        if(JS_ExecuteScript(cx, script, &result) != true) {
            fprintf(stderr, "Failed to execute script.\n");
            return 1;
        }

        // Give the GC a chance to run.
        JS_MaybeGC(cx);
    }

    JS_DestroyContext(cx);
    JS_ShutDown();

    return 0;
}
