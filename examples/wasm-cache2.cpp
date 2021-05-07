#include <cstdio>

#include <jsapi.h>
#include <js/CompilationAndEvaluation.h>
#include <js/SourceText.h>
#include <js/WasmModule.h>
#include <js/ArrayBuffer.h>
#include <js/BuildId.h>

#include "boilerplate.h"

unsigned char hi_wasm[] = {
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0a, 0x02, 0x60,
  0x01, 0x7f, 0x01, 0x7f, 0x60, 0x00, 0x01, 0x7f, 0x02, 0x0b, 0x01, 0x03,
  0x65, 0x6e, 0x76, 0x03, 0x62, 0x61, 0x72, 0x00, 0x00, 0x03, 0x02, 0x01,
  0x01, 0x07, 0x07, 0x01, 0x03, 0x66, 0x6f, 0x6f, 0x00, 0x01, 0x0a, 0x08,
  0x01, 0x06, 0x00, 0x41, 0x2a, 0x10, 0x00, 0x0b
};
unsigned int hi_wasm_len = 56;

static JSObject* Compile(JSContext* cx) {
    JSObject* arrayBuffer = JS::NewArrayBufferWithUserOwnedContents(cx, hi_wasm_len, hi_wasm);
    if (!arrayBuffer) return nullptr;
    JS::RootedObject buf(cx, arrayBuffer);

    return JS::CompileAndSerializeWasmModule(cx, buf);
}

static bool MyBuildId(JS::BuildIdCharVector* buildId) {
  const char buildid[] = "WASM-aot";
  return buildId->append(buildid, sizeof(buildid));
}

static bool BarFunc(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  args.rval().setInt32(args[0].toInt32());
  return true;
}

static bool WasmExample(JSContext* cx) {
  JS::RootedObject global(cx, boilerplate::CreateGlobal(cx));
  if (!global) {
    return false;
  }

  // Have to have BuildId.
  JS::SetProcessBuildIdOp(MyBuildId);

  JSAutoRealm ar(cx, global);

  // Construct Wasm module from AOT bytes.
  JS::RootedObject module_(cx);
  {
    JSObject* arrayBuffer = Compile(cx);
    if (!arrayBuffer) return false;
    JS::RootedObject buf(cx, arrayBuffer);

    JSObject* mod = JS::DeserializeWasmModule(cx, buf);
    if (!mod) return false;
    module_.set(mod);
  }

  // Get WebAssembly.Instance constructor.
  JS::RootedValue wasm(cx);
  JS::RootedValue wasmInstance(cx);
  if (!JS_GetProperty(cx, global, "WebAssembly", &wasm)) return false;
  JS::RootedObject wasmObj(cx, &wasm.toObject());
  if (!JS_GetProperty(cx, wasmObj, "Instance", &wasmInstance)) return false;

  // Construct Wasm module instance with required imports.
  JS::RootedObject instance_(cx);
  {
    // Build "env" imports object.
    JS::RootedObject envImportObj(cx, JS_NewPlainObject(cx));
    if (!envImportObj) return false;
    if (!JS_DefineFunction(cx, envImportObj, "bar", BarFunc, 1, 0)) return false;
    JS::RootedValue envImport(cx, JS::ObjectValue(*envImportObj));
    // Build imports bag.
    JS::RootedObject imports(cx, JS_NewPlainObject(cx));
    if (!imports) return false;
    if (!JS_SetProperty(cx, imports, "env", envImport)) return false;

    JS::RootedValueArray<2> args(cx);
    args[0].setObject(*module_.get()); // module
    args[1].setObject(*imports.get());// imports

    if (!Construct(cx, wasmInstance, args, &instance_)) return false;
  }

  // Find `foo` method in exports.
  JS::RootedValue exports(cx);
  if (!JS_GetProperty(cx, instance_, "exports", &exports)) return false;
  JS::RootedObject exportsObj(cx, &exports.toObject());
  JS::RootedValue foo(cx);
  if (!JS_GetProperty(cx, exportsObj, "foo", &foo)) return false;

  JS::RootedValue rval(cx);
  if (!Call(cx, JS::UndefinedHandleValue, foo, JS::HandleValueArray::empty(), &rval))
    return false;

  printf("The answer is %d\n", rval.toInt32());
  return true;
}

int main(int argc, const char* argv[]) {
  if (!boilerplate::RunExample(WasmExample)) {
    return 1;
  }
  return 0;
}
