#include "v8w.hpp"
#include "util.hpp"
#include <cstdio>
#include <cstring>
#include <ostream>
#include <fstream>

using namespace v8w;

namespace v8w{
    _handlers handlers;
};

#define GET_HANDLER_OR_FAIL(handler, str) \
{\
    auto x = util::get_export(L"process.exe", str);\
    if(!x){\
        printf("failed to get %s\n", #handler);\
        return false;\
    }\
    v8w::handlers.handler = (decltype(v8w::handlers.handler))x;\
}1


object::object(void* _o, void* _ctx, void* _isolate) : o(_o), ctx(_ctx), isolate(_isolate){

}

void object::iterate(std::function<bool(std::unique_ptr<object>)> f){

    uint64_t len = v8w::handlers.array_length(o); 
    if(!len){
        return;
    }

    //v8 defined array limit
    if(len >= 0x1FF80001){
        printf("array too long\n");
        return;
    }

    for(auto i=0u; i<len; ++i){
        void* out = 0;
        v8w::handlers.object_get_index(o, &out, ctx, i);
        if(!out){
            printf("got invalid object from index: %u\n", i);
            return;
        }

        if(f(std::make_unique<object>(out, ctx, isolate))){
            return;
        }
    }
}

void* object::get_context(){
    return ctx;
}

void* object::get_isolate(){
    return isolate;
}

void* object::getraw(){
    return o;
}

bool object::set(const std::string& attr, std::unique_ptr<object> ob){

    auto v8s_o = make_str(attr, ctx, isolate);
    if(!v8s_o){
        printf("1failed to set %s\n", attr.c_str());
        return false;
    }

    //set it 
    bool res = false;
    handlers.object_set(o, &res, ctx, v8s_o.value()->getraw(), ob->getraw());
    if(!res){
        printf("4failed to set %s\n", attr.c_str());
    }

    return res;
}

std::optional<std::unique_ptr<v8w::object>> object::get(std::string attr){
    auto v8s_o = make_str(attr, ctx, isolate);
    if(!v8s_o){
        return {};
    }
    return get(v8s_o.value().get());
}

std::optional<std::unique_ptr<object>> object::get(object* obj){
    void* ret = 0;
    handlers.object_get(o, &ret, ctx, obj->getraw());
    if(!ret){
        printf("failed to get obj from raw key\n");
        return {};
    }

    return std::make_unique<object>(ret, ctx, isolate);

}

std::optional<std::unique_ptr<object>> object::getchain(const std::vector<std::string>& attrs){
    object* obj = this;
    std::unique_ptr<object> obj_res = 0;
    for(auto& attr: attrs){
        auto x = obj->get(attr);
        if(!x){
            return {};
        }
        //printf("got: %s\n", attr.c_str());
        obj_res = std::move(x.value());
        obj = obj_res.get();
    }

    return obj_res;
}

std::optional<std::string> object::asstr(){
    void* stringified = 0;
    v8w::handlers.object_tostr(o, &stringified, ctx);
    if(!stringified){
        printf("failed stringify\n");
        return {};
    }

    auto len = v8w::handlers.string_lenutf8(stringified);
    if(!len){
        printf("failed get len2\n");
        return{};
    }

    auto s = std::string(len, 0);
    v8w::handlers.string_writeutf8(stringified, isolate, s.data(), len, 0, 1 | 2);
    return s;
}

std::optional<std::string> object::str(){

    auto len = v8w::handlers.string_lenutf8(o);
    if(!len){
        printf("failed get len2\n");
        return{};
    }

    auto s = std::string(len, 0);
    v8w::handlers.string_writeutf8(o, isolate, s.data(), len, 0, 1 | 2);
    return s;
}

std::optional<int64_t> object::integer(){
    if(!v8w::handlers.value_isnumber(o)){
        printf("object isnt an int\n");
        return {};
    } 

    __int128 justint = 0;
    v8w::handlers.value_tointeger(o, &justint, ctx);
    if(!(uint8_t)justint){
        printf("failed to cvrt to int\n");
        return {};
    }

    return *(((uint64_t*)&justint) + 1);
}

std::optional<bool> object::boolean(){
    if(!v8w::handlers.value_isbool(o)){
        printf("obj isnt a bool\n");
        return {};
    } 

    return v8w::handlers.value_tobool(o);
}

std::optional<std::string> object::stringify(){
    void* stringified = 0;
    v8w::handlers.json_stringify(&stringified, ctx, o, 0);
    if(!stringified){
        printf("failed stringify\n");
        return {};
    }

    auto len = v8w::handlers.string_lenutf8(stringified);
    if(!len){
        printf("failed get len\n");
        return{};
    }

    auto s = std::string(len, 0);

    v8w::handlers.string_writeutf8(stringified, isolate, s.data(), len, 0, 1 | 2);
    //printf("got %s\n", s); 
    return s;
}

void object::debug_print(){

    auto srepr = stringify();

    if(srepr){
        printf("%s\n", srepr.value().c_str());
    }else{
        printf("failed to debug printf\n");
    }
}

void object::debug_print_file(){
    auto srepr = stringify();
    if(srepr){
        auto os = std::ofstream("nothing-to-see-here.txt");
        os << srepr.value();
        os.close();
    }else{
        printf("failed to debug print file\n");
    }
}

std::optional<std::unique_ptr<v8w::object>> object::propnames(){

    void* propnames = 0;
    v8w::handlers.object_propertynames(o, &propnames, ctx);
    if(!propnames){
        printf("failed get propnames\n");
        return {};
    }

    return std::make_unique<object>(propnames, ctx, isolate);
}

std::optional<std::unique_ptr<object>> v8w::make_int(int32_t val, void* ctx, void* isolate){
    void* i = 0;
    handlers.integer_new(&i, isolate, val);
    if(!i){
        return {};
    }

    return std::make_unique<object>(i, ctx, isolate);
}

std::optional<std::unique_ptr<object>> v8w::make_str(const std::string& s, void* ctx, void* isolate){

    void* v8s = 0;
    handlers.string_newutf8(&v8s, isolate, (char*)s.data(), 0, s.length());
    if(!v8s){
        printf("failed cvrt str %s\n", s.c_str());
        return {};
    }

    return std::make_unique<object>(v8s, ctx, isolate);
}

std::optional<std::unique_ptr<object>> v8w::neww(void* ctx, void* isolate){

    void* out = 0;
    v8w::handlers.object_new(&out, isolate);
    if(!out){
        return {};
    }

    return std::make_unique<object>(out, ctx, isolate);
}

bool v8w::init(){

    GET_HANDLER_OR_FAIL(uv_run, "uv_run");

    //isolate
    GET_HANDLER_OR_FAIL(try_get_current, "TryGetCurrent");
    GET_HANDLER_OR_FAIL(get_current_context, "GetCurrentContext");
    GET_HANDLER_OR_FAIL(get_current_env, "GetCurrentEnvironment");
    GET_HANDLER_OR_FAIL(enter_context, "?Enter@Context@v8@@QEAAXXZ");
    GET_HANDLER_OR_FAIL(exit_context, "?Exit@Context@v8@@QEAAXXZ");
    GET_HANDLER_OR_FAIL(get_isolate, "?GetIsolate@Context@v8@@QEAAPEAVIsolate@2@XZ");


    GET_HANDLER_OR_FAIL(handle_create, "??0HandleScope@v8@@QEAA@PEAVIsolate@1@@Z");
    GET_HANDLER_OR_FAIL(handle_destroy, "??1HandleScope@v8@@QEAA@XZ");
    
    GET_HANDLER_OR_FAIL(get_context_global, "?Global@Context@v8@@QEAA?AV?$Local@VObject@v8@@@2@XZ");
    GET_HANDLER_OR_FAIL(get_creation_context,"?GetCreationContext@Object@v8@@QEAA?AV?$MaybeLocal@VContext@v8@@@2@PEAVIsolate@2@@Z");

    GET_HANDLER_OR_FAIL(object_propertynames, "?GetPropertyNames@Object@v8@@QEAA?AV?$MaybeLocal@VArray@v8@@@2@V?");
    GET_HANDLER_OR_FAIL(object_tostr, "?ObjectProtoToString@Object@v8@@QEAA?AV?$MaybeLocal@VString@v8@@@");
    GET_HANDLER_OR_FAIL(object_get, "?Get@Object@v8@@QEAA?AV?$MaybeLocal@VValue@v8@@@2@V?$Local@VContext@v8@@@2@V?$Local@VValue@v8@@@2@@Z");
    GET_HANDLER_OR_FAIL(object_get_index, "?Get@Object@v8@@QEAA?AV?$MaybeLocal@VValue@v8@@@2@V?$Local@VContext@v8@@@2@I@Z");
    GET_HANDLER_OR_FAIL(object_set, "?Set@Object@v8@@QEAA?AV?$Maybe@_N@2@V?$Local@VContext@v8@@@2@V?$Local@VValue@v8@@@2@1@Z");
 
    GET_HANDLER_OR_FAIL(object_call, "?Call@Function@v8@@QEAA?AV?$MaybeLocal@VValue@v8@@@2@V?$Local@VContext@v8@@@2@V?$Local@VValue@v8@@@2@HQEAV52@@Z");
    GET_HANDLER_OR_FAIL(object_isfunc, "?IsFunction@Value@v8@@QEBA_NXZ");
    GET_HANDLER_OR_FAIL(object_new, "?New@Object@v8@@SA?AV?$Local@VObject@v8@@@2@PEAVIsolate@2@@Z");

    GET_HANDLER_OR_FAIL(value_isnumber, "?IsNumber@Value@v8@@QEBA_NXZ");
    GET_HANDLER_OR_FAIL(value_tointeger, "?IntegerValue@Value@v8@@QEBA?AV?$Maybe@_J@2@V?$Local@VContext@v8@@@2@@Z");

    GET_HANDLER_OR_FAIL(value_isbool, "?IsBoolean@Value@v8@@QEBA_NXZ");
    GET_HANDLER_OR_FAIL(value_tobool, "?Value@Boolean@v8@@QEBA_NXZ");

    GET_HANDLER_OR_FAIL(array_length, "?Length@Array@v8@@QEBAIXZ");
    GET_HANDLER_OR_FAIL(array_iterate, "?Iterate@Array@v8@@QEAA?AV?$Maybe@X@2@V?$Local@VContext@v8@@@2@P6");


    GET_HANDLER_OR_FAIL(json_stringify, "?Stringify@JSON@v8@@SA?AV?$MaybeLocal@VString@v8@@@2@V?$Local@VCo");

    GET_HANDLER_OR_FAIL(string_lenutf8, "?Utf8Length@String@v8@@QEBAHPEAVIsolate@2@@Z");
    GET_HANDLER_OR_FAIL(string_writeutf8, "?WriteUtf8@String@v8@@QEBAHPEAVIsolate@2@PEADHPEAHH@Z");
    GET_HANDLER_OR_FAIL(string_newutf8, "?NewFromUtf8Literal@String@v8@@CA?AV?$Local@VString@v8@@@2@PEAVIs");

    GET_HANDLER_OR_FAIL(integer_new, "?New@Integer@v8@@SA?AV?$Local@VInteger@v8@@@2@PEAVIsolate@2@H@Z");
    GET_HANDLER_OR_FAIL(trycatch, "??0TryCatch@v8@@QEAA@PEAVIsolate@1@@Z");
    GET_HANDLER_OR_FAIL(trycatch_destroy, "??1TryCatch@v8@@QEAA@XZ");

    GET_HANDLER_OR_FAIL(trycatch_hascaught, "?HasCaught@TryCatch@v8@@QEBA_NXZ");
    GET_HANDLER_OR_FAIL(trycatch_message, "?Message@TryCatch@v8@@QEBA?AV?$Local@VMessage@v8@@@2@XZ");
    GET_HANDLER_OR_FAIL(message_get, "?Get@Message@v8@@QEBA?AV?$Local@VString@v8@@@2@XZ");

    return true; 
}