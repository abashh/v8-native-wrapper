#pragma once
#include <stdint.h>
#include <string>
#include <optional>
#include <vector>
#include <memory>
#include <functional>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <processthreadsapi.h>
#include <WinUser.h>
#include <winternl.h>

namespace v8w{

    class object;

    std::optional<std::unique_ptr<object>> make_int(int32_t val, void* ctx, void* isolate);
    std::optional<std::unique_ptr<object>> make_str(const std::string& s, void* ctx, void* isolate);
    std::optional<std::unique_ptr<object>> neww(void* ctx, void* isolate);
    bool init();

    struct _handlers{
        
        //uv
        int (*uv_run)(void*, uint64_t);
        
        //tls stuff
        void*(*try_get_current)();
        void*(*get_current_context)(void* isolate, void** out);
        void*(*get_current_env)(void* ctx);
        void (*enter_context)(void* out);
        void(*exit_context)(void* ctx);
        void* (*get_isolate)(void* ctx);

        void*(*handle_create)(void** out, void* isolate);
        void(*handle_destroy)(void* handle);

        //context
        void*(*get_context_global)(void* ctx, void** out);
        void*(*get_creation_context)(void* ctx);

        //object
        void*(*object_propertynames)(void* obj, void** out, void* ctx);
        void*(*object_tostr)(void* obj, void** out, void* ctx);
        void*(*object_get)(void* obj, void** out, void* ctx, void* key);
        void*(*object_get_index)(void* obj, void** out, void* ctx, uint32_t index);
        void*(*object_set)(void* obj, bool* out, void* ctx, void* key, void* val);

        void*(*object_call)(void* obj, void** out, void* ctx, void* recv, int argc, void* argv);
        bool(*object_isfunc)(void* obj);
        void*(*object_new)(void** out, void* isolate);

        //value
        bool(*value_isnumber)(void* value);
        void*(*value_tointeger)(void* value, __int128* out, void* ctx);
        bool(*value_isbool)(void* value);
        bool(*value_tobool)(void* value);

        //array
        uint64_t(*array_length)(void* array);
        void*(*array_iterate)(void* array, void** out, void* ctx, uint64_t(*cb)(uint32_t idx, void* ele, void* data), void* data);


        void*(*json_stringify)(void** out, void* ctx, void* obj, void* useless);

        //string
        //// Indicates that the output string should be null-terminated. In that
        //// case, the output buffer must include sufficient space for the
        //// additional null character.
        //kNullTerminate = 1,
        //// Used by WriteUtf8 to replace orphan surrogate code units with the
        //// unicode replacement character. Needs to be set to guarantee valid UTF-8
        //// output.
        //kReplaceInvalidUtf8 = 2
        int(*string_writeutf8)(void* str, void* isolate, void* buffer, int len, void* useless, int options);
        uint32_t(*string_lenutf8)(void* str);

        ///**
        // * Create a new string, always allocating new storage memory.
        // */
        //kNormal,

        ///**
        // * Acts as a hint that the string should be created in the
        // * old generation heap space and be deduplicated if an identical string
        // * already exists.
        // */
        //kInternalized
        uint32_t(*string_newutf8)(void** out, void* isolate, char* literal, int options, int len);


        void*(*integer_new)(void** out, void* isolate, int val);


        void*(*trycatch)(void** out, void* isolate);
        void(*trycatch_destroy)(void* trycatch);
        bool(*trycatch_hascaught)(void* trycatch);
        void*(*trycatch_message)(void* trycatch, void** out);

        void*(*message_get)(void* message, void** out);
    };

    extern _handlers handlers;

    class object{

        public:
        object(void* _o, void* _ctx, void* _isolate);

        void iterate(std::function<bool(std::unique_ptr<object>)> f);

        std::optional<std::unique_ptr<object>> call(void* thiss = 0){

            if(!handlers.object_isfunc(o)){
                printf("obj is not a func\n");
                return {};
            }

            void* ret = 0;
            handlers.object_call(o, &ret, ctx, thiss ? thiss :(void*)((uint64_t)isolate + 720), 0, 0);
            if(!ret){
                printf("failed call\n");
                return {};
            }

            return std::make_unique<object>(ret, ctx, isolate);
        }


        template<uint32_t N>
        std::optional<std::unique_ptr<object>> call(void*(&argv)[N], void* thiss = 0){

            //void*(*object_call)(void* obj, void** out, void* ctx, void* recv, int argc, void* argv);

            if(!handlers.object_isfunc(o)){
                printf("obj is not a func\n");
                return {};
            }

            void* ret = 0;
            handlers.object_call(o, &ret, ctx, thiss ? thiss : (void*)((uint64_t)isolate + 720), N, N ? argv : 0);
            if(!ret){
                printf("failed call\n");
                return {};
            }

            return std::make_unique<object>(ret, ctx, isolate);
        }

        void* get_context();
        void* get_isolate();
        void* getraw();


        bool set(const std::string& attr, std::unique_ptr<object> ob);
        template<typename T>
        bool set(const std::string& attr, T val){

            std::unique_ptr<v8w::object> ob;
            if constexpr(std::is_same<T, std::string>::value) {
                auto x = make_str(val, ctx, isolate);
                if(!x){
                    printf("2failed to set %s\n", attr.c_str());
                    return false;
                }
                ob = std::move(x.value());
            }else if constexpr(std::is_same<T, int32_t>::value){
                auto x = make_int(val, ctx, isolate);
                if(!x){
                    printf("3failed to set %s\n", attr.c_str());
                    return false;
                }
                ob = std::move(x.value());
            }else{
                static_assert(false, "invalid type");
            }

            return set(attr, std::move(ob));
        }

        template<typename T>
        std::optional<T> get(std::string attr){
            auto obj = get(attr);
            if(!obj){
                return {};
            }
            if constexpr(std::is_same<T, std::string>::value) {
                return obj.value()->str(); 
            }else if constexpr(std::is_same<T, int64_t>::value){
                return obj.value()->integer(); 
            }else if constexpr(std::is_same<T, bool>::value){
                return obj.value()->boolean(); 
            }else{
                static_assert(false, "invalid type");
            }
            return {};
        }
        std::optional<std::unique_ptr<v8w::object>> get(std::string attr);
        std::optional<std::unique_ptr<object>> get(object* o);
        std::optional<std::unique_ptr<object>> getchain(const std::vector<std::string>& attrs);
        std::optional<std::string> asstr();
        std::optional<std::string> str();
        std::optional<int64_t> integer();
        std::optional<bool> boolean();
        std::optional<std::string> stringify();
        void debug_print();
        void debug_print_file();
        
        std::optional<std::unique_ptr<object>> propnames();


        private:
        void* o;
        void* ctx;
        void* isolate;
    };

    class TryCatch{
        public:
        TryCatch(void* ctx, void* isolate): _ctx(ctx), _isolate(isolate){
            v8w::handlers.trycatch((void**)&_trycatch[0], isolate);
        }
        ~TryCatch(){
            v8w::handlers.trycatch_destroy(&_trycatch[0]);
        }

        std::optional<std::string> message(){

            void* msg = 0;
            v8w::handlers.trycatch_message(&_trycatch[0], &msg);
            if(!msg){
                return {};
            }

            void* msg_s = 0;
            v8w::handlers.message_get(msg, &msg_s);
            if(!msg_s){
                return {};
            }
            return v8w::object(msg_s, _ctx, _isolate).str();
        }

        private:
        char _trycatch[200];
        void* _ctx;
        void* _isolate;
    };

    class HandleScope{
        public:
        HandleScope(void* isolate){
            v8w::handlers.handle_create((void**)&_scope[0], isolate);
        }
        ~HandleScope(){
            v8w::handlers.handle_destroy(&_scope[0]);
        }
        private:
        char _scope[200];
    };

    class Context{

        public:
        Context(void* ctx): _ctx(ctx){
            v8w::handlers.enter_context(ctx);
        }

        ~Context(){
            v8w::handlers.exit_context(_ctx);
        }

        private:
        void* _ctx;
    };

};