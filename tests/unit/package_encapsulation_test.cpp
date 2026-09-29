#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "strut/cli.h"
#include "strut/package.h"
#include "temp_directory.h"

namespace {
void require(bool value, const char* message, const std::string& detail = {}) {
    if (!value) { std::cerr << "FAIL: " << message << '\n' << detail; std::exit(1); }
}

void write(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << text;
}

std::filesystem::path cache_package(const std::filesystem::path& root, const std::string& name, const std::string& source,
                                    const std::vector<std::pair<std::string,std::string>>& files = {},
                                    const std::vector<std::string>& dependencies = {}) {
    const auto package=root/("source-"+name);std::filesystem::create_directories(package);
    std::ostringstream manifest;manifest<<"{\"name\":\""<<name<<"\",\"version\":\"1.0.0\",\"entry\":\"main.p\",\"dependencies\":{";
    for(std::size_t i=0;i<dependencies.size();++i){if(i)manifest<<',';manifest<<'"'<<dependencies[i]<<"\":\"1.0.0\"";}manifest<<"}}";
    write(package/"strut.json",manifest.str());write(package/"main.p",source);for(const auto& file:files)write(package/file.first,file.second);
    std::filesystem::path cached;strut::PackageManifest metadata;std::string error;require(strut::cache_local_package(package,cached,metadata,error),("cache "+name).c_str(),error);
    return cached;
}

void configure(const std::filesystem::path& root, const std::vector<std::string>& dependencies) {
    strut::PackageManifest project;project.name="encapsulation-test";project.version="0.1.0";project.entry="app.p";for(const auto& dependency:dependencies)project.dependencies[dependency]="1.0.0";
    std::string error;require(strut::write_package_manifest_file(root/"strut.json",project,error),"write project manifest",error);require(strut::write_lockfile(root,project,error),"write project lock",error);
}

std::string check(const std::filesystem::path& root, const std::string& source, bool success) {
    write(root/"app.p",source);std::string executable="strut",option="--check",path=(root/"app.p").string();char* argv[]={executable.data(),option.data(),path.data()};std::ostringstream out,err;const int status=strut::run_cli(3,argv,out,err);require((status==0)==success,success?"expected package check success":"expected package check failure",source+"\n"+err.str());return err.str();
}

void compile(const std::filesystem::path& root) {
    std::string executable="strut",path=(root/"app.p").string(),output_option="-o",output=(root/"encapsulation-app").string();
#ifdef _WIN32
    output += ".exe";
#endif
    char* argv[]={executable.data(),path.data(),output_option.data(),output.data()};std::ostringstream out,err;require(strut::run_cli(4,argv,out,err)==0,"compile encapsulated packages",err.str());require(std::filesystem::exists(output),"encapsulated package output exists");
}

std::string quote(const std::filesystem::path& path) { return "\""+path.string()+"\""; }

std::string read(const std::filesystem::path& path) {
    std::ifstream input(path);return {std::istreambuf_iterator<char>(input),{}};
}

void compile_and_run(const std::filesystem::path& root, const std::string& expected) {
    compile(root);auto executable=root/"encapsulation-app";
#ifdef _WIN32
    executable += ".exe";
#endif
    const auto output=root/"run.out";
    require(std::system((quote(executable)+" > "+quote(output)).c_str())==0,"run encapsulated package program");
    require(read(output)==expected,"encapsulated package runtime output",read(output));
}

std::string identifier_hex(const std::string& value) {
    static constexpr char digits[]="0123456789abcdef";std::string out;
    for(unsigned char c:value){out+=digits[c>>4];out+=digits[c&15];}return out;
}
}

int main() {
    TestTempDirectory temp("strut-package-encapsulation");const auto root=temp.path(),home=root/"home";
#ifdef _WIN32
    _putenv_s("STRUT_HOME",home.string().c_str());
#else
    setenv("STRUT_HOME",home.string().c_str(),1);
#endif

    cache_package(root,"encap",
        "include \"internal.p\";\nstruct Carrier { int value; }\nCarrier facade := Carrier { value: 7 };\nfunction api(int value) -> int { return helper() + value; }\nfunction api(string value) -> int { return helper(); }\nexport facade;\nexport api;\n",
        {{"internal.p","function helper() -> int { return 2; }\n"}});
    configure(root,{"encap"});
    check(root,"include <encap>;\nfunction helper() -> int { return 40; }\nfunction main() -> int { return api(1) + helper(); }\n",true);
    auto hidden=check(root,"include <encap>;\nfunction main() -> int { return helper(); }\n",false);require(hidden.find("unknown callable 'helper'")!=std::string::npos,"private symbol diagnostic",hidden);

    cache_package(root,"legacy","function legacy_api() -> int { return 1; }\n");configure(root,{"legacy"});check(root,"include <legacy>;\nfunction main() -> int { return legacy_api(); }\n",true);
    cache_package(root,"legacy-main","function main() -> int { return 99; }\nfunction legacy_value() -> int { return 4; }\n");configure(root,{"legacy-main"});check(root,"include <legacy-main>;\nfunction main() -> void { println(legacy_value()); return; }\n",true);compile_and_run(root,"4\n");
    cache_package(root,"empty","function hidden() -> int { return 1; }\nexport;\n");configure(root,{"empty"});auto empty=check(root,"include <empty>;\nfunction main() -> int { return hidden(); }\n",false);require(empty.find("unknown callable 'hidden'")!=std::string::npos,"export-empty package",empty);

    cache_package(root,"missing","function present() -> int { return 1; }\nexport absent;\n");configure(root,{"missing"});auto missing=check(root,"include <missing>;\nfunction main() -> int { return 0; }\n",false);require(missing.find("does not name a symbol owned")!=std::string::npos,"missing export diagnostic",missing);
    cache_package(root,"duplicate","function value() -> int { return 1; }\nexport value;\nexport value;\n");configure(root,{"duplicate"});auto duplicate=check(root,"include <duplicate>;\nfunction main() -> int { return 0; }\n",false);require(duplicate.find("duplicate export 'value'")!=std::string::npos,"duplicate export diagnostic",duplicate);
    cache_package(root,"ambiguous","struct Thing { int value; }\nfunction Thing() -> int { return 1; }\nexport Thing;\n");configure(root,{"ambiguous"});auto ambiguous=check(root,"include <ambiguous>;\nfunction main() -> int { return 0; }\n",false);require(ambiguous.find("export 'Thing' is ambiguous")!=std::string::npos,"ambiguous export diagnostic",ambiguous);
    cache_package(root,"private-signature","struct Hidden { int value; }\nfunction expose(Hidden value) -> int { return value.value; }\nexport expose;\n");configure(root,{"private-signature"});auto signature=check(root,"include <private-signature>;\nfunction main() -> int { return 0; }\n",false);require(signature.find("exposes private type 'Hidden'")!=std::string::npos,"private signature diagnostic",signature);
    cache_package(root,"private-field","struct Hidden { int value; }\nstruct Public { Hidden hidden; }\nexport Public;\n");configure(root,{"private-field"});auto field_leak=check(root,"include <private-field>;\nfunction main() -> int { return 0; }\n",false);require(field_leak.find("exposes private type 'Hidden'")!=std::string::npos,"private field diagnostic",field_leak);
    cache_package(root,"private-base","struct HiddenBase { int value; }\nstruct Public : HiddenBase {}\nexport Public;\n");configure(root,{"private-base"});auto base_leak=check(root,"include <private-base>;\nfunction main() -> int { return 0; }\n",false);require(base_leak.find("exposes private type 'HiddenBase'")!=std::string::npos,"private base diagnostic",base_leak);
    cache_package(root,"private-error","error HiddenError { string message; int code; }\nfunction risky() -> int : HiddenError { throw HiddenError { message: \"x\", code: 1 }; }\nexport risky;\n");configure(root,{"private-error"});auto error_leak=check(root,"include <private-error>;\nfunction main() -> int { return 0; }\n",false);require(error_leak.find("exposes private type 'HiddenError'")!=std::string::npos,"private checked error diagnostic",error_leak);
    cache_package(root,"generic-shadow","struct T { int value; }\nfunction identity[T](T value) -> T { return value; }\nexport identity;\n");configure(root,{"generic-shadow"});check(root,"include <generic-shadow>;\nfunction main() -> int { return identity(7); }\n",true);

    cache_package(root,"child","function child_api() -> int { return 3; }\nexport child_api;\n");
    cache_package(root,"parent","include <child>;\nfunction parent_api() -> int { return child_api(); }\nexport parent_api;\n",{}, {"child"});configure(root,{"parent"});check(root,"include <parent>;\nfunction main() -> int { return parent_api(); }\n",true);
    auto transitive=check(root,"include <parent>;\nfunction main() -> int { return child_api(); }\n",false);require(transitive.find("unknown callable 'child_api'")!=std::string::npos,"transitive export isolation",transitive);
    check(root,"include <parent>;\nfunction child_api() -> int { return 8; }\nfunction main() -> int { return child_api() + parent_api(); }\n",true);
    auto transitive_include=check(root,"include <parent>;\ninclude <child>;\nfunction main() -> int { return 0; }\n",false);require(transitive_include.find("is not a declared dependency of 'encapsulation-test'")!=std::string::npos,"transitive package cannot be directly included without declaration",transitive_include);
    cache_package(root,"undeclared-parent","include <child>;\nfunction parent_api() -> int { return child_api(); }\nexport parent_api;\n");configure(root,{"undeclared-parent"});auto undeclared_dependency=check(root,"include <undeclared-parent>;\nfunction main() -> int { return 0; }\n",false);require(undeclared_dependency.find("is not a declared dependency of 'undeclared-parent'")!=std::string::npos,"package import requires package manifest dependency",undeclared_dependency);
    cache_package(root,"reexport","include <child>;\nexport child_api;\n",{}, {"child"});configure(root,{"reexport"});auto reexport=check(root,"include <reexport>;\nfunction main() -> int { return 0; }\n",false);require(reexport.find("does not name a symbol owned")!=std::string::npos,"direct re-export rejection",reexport);
    cache_package(root,"typed-child","struct ChildType { int value; }\nexport ChildType;\n");
    cache_package(root,"typed-parent","include <typed-child>;\nfunction expose(ChildType value) -> ChildType { return value; }\nexport expose;\n",{}, {"typed-child"});
    configure(root,{"typed-parent"});
    auto dependency_signature=check(root,"include <typed-parent>;\nfunction main() -> int { return 0; }\n",false);
    require(dependency_signature.find("exposes dependency type 'ChildType'")!=std::string::npos,"dependency type is not re-exported through signature",dependency_signature);

    configure(root,{"encap"});auto subpath=check(root,"include <encap/internal.p>;\nfunction main() -> int { return 0; }\n",false);require(subpath.find("does not allow external subpath includes")!=std::string::npos,"explicit package subpath rejection",subpath);
    cache_package(root,"module-owner","include <filesystem>;\nfunction package_exists(string path) -> bool : FilesystemError { return exists(path); }\nexport package_exists;\n");configure(root,{"module-owner"});check(root,"include <module-owner>;\nfunction main() -> int : FilesystemError { if (package_exists(\".\")) { return 0; } return 1; }\n",true);auto module=check(root,"include <module-owner>;\nfunction main() -> int : FilesystemError { if (exists(\".\")) { return 0; } return 1; }\n",false);require(module.find("requires standard module <filesystem>")!=std::string::npos,"owner-local standard module",module);

    cache_package(root,"left","function helper() -> int { return 1; }\nfunction left_api() -> int { return helper(); }\nexport left_api;\n");cache_package(root,"right","function helper() -> int { return 2; }\nfunction right_api() -> int { return helper(); }\nexport right_api;\n");configure(root,{"left","right"});check(root,"include <left>;\ninclude <right>;\nfunction helper() -> int { return 4; }\nfunction main() -> int { return left_api() + right_api() + helper(); }\n",true);compile(root);
    cache_package(root,"amb-a","function same() -> int { return 1; }\nexport same;\n");cache_package(root,"amb-b","function same() -> int { return 2; }\nexport same;\n");configure(root,{"amb-a","amb-b"});auto import_ambiguity=check(root,"include <amb-a>;\ninclude <amb-b>;\nfunction main() -> int { return 0; }\n",false);require(import_ambiguity.find("ambiguous imported symbol 'same'")!=std::string::npos,"direct import ambiguity diagnostic",import_ambiguity);
    check(root,"include <amb-a>;\ninclude <amb-b>;\nfunction same() -> int { return 3; }\nfunction main() -> int { return same(); }\n",true);
    cache_package(root,"amb-shadow","include <amb-a>;\ninclude <amb-b>;\nfunction same() -> int { return 4; }\nfunction shadow_api() -> int { return same(); }\nexport shadow_api;\n",{}, {"amb-a","amb-b"});configure(root,{"amb-shadow"});check(root,"include <amb-shadow>;\nfunction main() -> int { return shadow_api(); }\n",true);

    configure(root,{"encap"});check(root,"include <encap>;\nfunction main() -> void { println(api(1) + api(\"x\")); return; }\n",true);compile_and_run(root,"5\n");

    cache_package(root,"public-kinds",
        "answer := 9;\n"
        "enum Mode { ready }\n"
        "error PublicError { string message; int code; }\n"
        "function fail() -> int : PublicError { throw PublicError { message: \"x\", code: 1 }; }\n"
        "export answer;\nexport Mode;\nexport PublicError;\nexport fail;\n");
    configure(root,{"public-kinds"});check(root,"include <public-kinds>;\nfunction main() -> void { Mode mode := Mode::ready; if (mode == Mode::ready) { try { fail(); } catch (PublicError err) { println(answer); } } return; }\n",true);compile_and_run(root,"9\n");

    cache_package(root,"public-type",
        "struct PublicValue {\n"
        "  int value;\n"
        "  function get() -> int { return value; }\n"
        "}\n"
        "type PublicAlias := PublicValue;\n"
        "export PublicValue;\n"
        "export PublicAlias;\n");
    configure(root,{"public-type"});
    check(root,
        "include <public-type>;\n"
        "function main() -> void {\n"
        "  PublicAlias item := PublicValue { value: 6 };\n"
        "  println(item.get());\n"
        "  return;\n"
        "}\n",true);
    compile_and_run(root,"6\n");

    cache_package(root,"facade",
        "value := 100;\n"
        "struct Carrier {\n"
        "  int value;\n"
        "  function add(int amount) -> int;\n"
        "}\n"
        "function Carrier::add(int amount) -> int { return value + amount; }\n"
        "function make_carrier() -> Carrier { return Carrier { value: 7 }; }\n"
        "facade := make_carrier();\n"
        "export facade;\n");
    configure(root,{"facade"});
    check(root,
        "include <facade>;\n"
        "function main() -> void { println(facade.add(5)); return; }\n",true);
    compile_and_run(root,"12\n");
    auto method_global=check(root,"include <facade>;\nfunction main() -> int { facade.add(5); return add(5); }\n",false);require(method_global.find("unknown callable 'add'")!=std::string::npos,"out-of-struct method is not a global function",method_global);
    cache_package(root,"inherited-field",
        "value := 100;\n"
        "struct Base { int value; }\n"
        "struct Middle : Base {}\n"
        "struct Carrier : Middle { function read() -> int { return value; } }\n"
        "function inherited_value() -> int { Carrier item := Carrier { value: 7 }; return item.read(); }\n"
        "export inherited_value;\n");
    configure(root,{"inherited-field"});check(root,"include <inherited-field>;\nfunction main() -> void { println(inherited_value()); return; }\n",true);compile_and_run(root,"7\n");
    cache_package(root,"forwarded-facade",
        "struct Carrier { int value; function read() -> int { return value; } }\n"
        "Carrier base := Carrier { value: 11 };\n"
        "facade := (base);\n"
        "export facade;\n");
    configure(root,{"forwarded-facade"});check(root,"include <forwarded-facade>;\nfunction main() -> void { println(facade.read()); return; }\n",true);compile_and_run(root,"11\n");
    cache_package(root,"unknown-carrier","facade := () => 1;\nexport facade;\n");configure(root,{"unknown-carrier"});auto unknown_carrier=check(root,"include <unknown-carrier>;\nfunction main() -> int { return 0; }\n",false);require(unknown_carrier.find("unable to determine public type of exported value 'facade'")!=std::string::npos,"unknown exported carrier rejected",unknown_carrier);

    cache_package(root,"bad-facade",
        "struct Secret { int value; }\n"
        "struct Carrier {\n"
        "  function leak(map<string, Secret?[]> values) -> Secret;\n"
        "}\n"
        "Carrier facade := Carrier {};\n"
        "export facade;\n");
    configure(root,{"bad-facade"});
    auto facade_leak=check(root,"include <bad-facade>;\nfunction main() -> int { return 0; }\n",false);
    require(facade_leak.find("exported facade 'facade' exposes private type 'Secret'")!=std::string::npos,"facade method private type diagnostic",facade_leak);

    cache_package(root,"bad-public-method",
        "struct Secret { int value; }\n"
        "struct PublicApi { function leak() -> vector<Secret?>; }\n"
        "export PublicApi;\n");
    configure(root,{"bad-public-method"});
    auto public_method_leak=check(root,"include <bad-public-method>;\nfunction main() -> int { return 0; }\n",false);
    require(public_method_leak.find("exported symbol 'PublicApi' exposes private type 'Secret'")!=std::string::npos,"exported struct method private type diagnostic",public_method_leak);
    cache_package(root,"cross-method-generic","struct T { int value; }\nstruct PublicApi { function generic[T](T value) -> T; function leak() -> T; }\nexport PublicApi;\n");configure(root,{"cross-method-generic"});auto cross_method=check(root,"include <cross-method-generic>;\nfunction main() -> int { return 0; }\n",false);require(cross_method.find("exposes private type 'T'")!=std::string::npos,"one method generic cannot mask another method leak",cross_method);
    cache_package(root,"alias-facade-leak","struct Secret { int value; }\nstruct Carrier { Secret secret; }\ntype CarrierAlias := Carrier;\nCarrierAlias facade := Carrier { secret: Secret { value: 1 } };\nexport facade;\n");configure(root,{"alias-facade-leak"});auto alias_facade=check(root,"include <alias-facade-leak>;\nfunction main() -> int { return 0; }\n",false);require(alias_facade.find("exported facade 'facade' exposes private type 'Secret'")!=std::string::npos,"private alias facade carrier is recursively validated",alias_facade);
    cache_package(root,"generic-facade","struct Carrier[T] { T value; function present() -> bool { return true; } }\nCarrier<int> facade;\nexport facade;\n");configure(root,{"generic-facade"});check(root,"include <generic-facade>;\nfunction main() -> int { facade.present(); return 0; }\n",true);
    cache_package(root,"generic-facade-leak","struct Secret { int value; }\nstruct Carrier[T] { T value; }\nCarrier<Secret> facade;\nexport facade;\n");configure(root,{"generic-facade-leak"});auto generic_facade=check(root,"include <generic-facade-leak>;\nfunction main() -> int { return 0; }\n",false);require(generic_facade.find("exported facade 'facade' exposes private type 'Secret'")!=std::string::npos,"private generic facade argument diagnostic",generic_facade);

    cache_package(root,"shadowing",
        "shadow := 90;\n"
        "error ShadowError { string message; int code; }\n"
        "function parameter_shadow(int shadow) -> int { return shadow; }\n"
        "function local_shadow() -> int { int shadow := 2; return shadow; }\n"
        "function lambda_shadow() -> int { function<(int)->int> f := (int shadow) => shadow; return f(3); }\n"
        "function loop_shadow() -> int { total := 0; for (int shadow := 0; shadow < 2; shadow++) { total += shadow; } return total; }\n"
        "function range_shadow() -> int { total := 0; for (shadow : [4, 5]) { total += shadow; } return total; }\n"
        "function catch_shadow() -> int { try { throw ShadowError { message: \"x\", code: 6 }; } catch (ShadowError shadow) { return shadow.code; } }\n"
        "function all_shadowing() -> int { return parameter_shadow(1) + local_shadow() + lambda_shadow() + loop_shadow() + range_shadow() + catch_shadow(); }\n"
        "export all_shadowing;\n");
    configure(root,{"shadowing"});
    check(root,"include <shadowing>;\nfunction main() -> void { println(all_shadowing()); return; }\n",true);
    compile_and_run(root,"22\n");

    cache_package(root,"private-operator",
        "struct Number { int value; }\n"
        "operator +(Number left, Number right) -> Number { return Number { value: left.value + right.value }; }\n"
        "operator ==(Number left, Number right) -> bool { return left.value == right.value; }\n"
        "operator -(Number value) -> Number { return Number { value: 0 - value.value }; }\n"
        "function operator_result() -> int { Number left := Number { value: 2 }; Number right := Number { value: 3 }; if (left == right) { return 0; } return (left + -right).value; }\n"
        "export operator_result;\n");
    configure(root,{"private-operator"});
    check(root,"include <private-operator>;\nfunction main() -> void { println(operator_result()); return; }\n",true);
    compile_and_run(root,"-1\n");
    cache_package(root,"public-operator",
        "struct Number { int value; }\n"
        "operator +(Number left, Number right) -> Number { return Number { value: left.value + right.value }; }\n"
        "operator ==(Number left, Number right) -> bool { return left.value == right.value; }\n"
        "operator -(Number value) -> Number { return Number { value: 0 - value.value }; }\n"
        "export Number;\n");
    configure(root,{"public-operator"});auto external_operator=check(root,"include <public-operator>;\nfunction main() -> int { Number left := Number { value: 1 }; Number right := Number { value: 2 }; result := left + right; return 0; }\n",false);require(external_operator.find("no infix + overload exists")!=std::string::npos,"package operator remains private to its owner",external_operator);
    auto typed_operator=check(root,"include <public-operator>;\nfunction main() -> int { Number left := Number { value: 1 }; Number right := Number { value: 2 }; Number result := left + right; return 0; }\n",false);require(typed_operator.find("no infix + overload exists")!=std::string::npos,"typed initialization diagnoses inaccessible package operator",typed_operator);
    auto assigned_operator=check(root,"include <public-operator>;\nfunction main() -> int { Number left := Number { value: 1 }; Number right := Number { value: 2 }; Number result := left; result = left + right; return 0; }\n",false);require(assigned_operator.find("no infix + overload exists")!=std::string::npos,"assignment diagnoses inaccessible package operator",assigned_operator);
    auto returned_operator=check(root,"include <public-operator>;\nfunction combine(Number left, Number right) -> Number { return left + right; }\nfunction main() -> int { return 0; }\n",false);require(returned_operator.find("no infix + overload exists")!=std::string::npos,"return diagnoses inaccessible package operator",returned_operator);
    auto discarded_operator=check(root,"include <public-operator>;\nfunction main() -> int { Number left := Number { value: 1 }; Number right := Number { value: 2 }; left + right; return 0; }\n",false);require(discarded_operator.find("no infix + overload exists")!=std::string::npos,"discarded expression diagnoses inaccessible package operator",discarded_operator);
    auto comparison_operator=check(root,"include <public-operator>;\nfunction main() -> int { Number left := Number { value: 1 }; Number right := Number { value: 2 }; if (left == right) { return 1; } return 0; }\n",false);require(comparison_operator.find("no infix == overload exists")!=std::string::npos,"comparison diagnoses inaccessible package operator",comparison_operator);
    auto unary_operator=check(root,"include <public-operator>;\nfunction main() -> int { Number value := Number { value: 1 }; Number result := -value; return 0; }\n",false);require(unary_operator.find("no prefix - overload exists")!=std::string::npos,"unary expression diagnoses inaccessible package operator",unary_operator);

    cache_package(root,"a-b",
        "struct Helper { int value; }\n"
        "seed := 1;\n"
        "function dashed() -> int { Helper item := Helper { value: seed }; return item.value; }\n"
        "export dashed;\n");
    cache_package(root,"a_b",
        "struct Helper { int value; }\n"
        "seed := 2;\n"
        "function underscored() -> int { Helper item := Helper { value: seed }; return item.value; }\n"
        "export underscored;\n");
    configure(root,{"a-b","a_b"});
    check(root,
        "include <a-b>;\ninclude <a_b>;\n"
        "struct Helper { int value; }\nseed := 3;\n"
        "function main() -> void { Helper item := Helper { value: seed }; println(dashed() + underscored() + item.value); return; }\n",true);
    compile_and_run(root,"6\n");

    const auto guessed_cache=cache_package(root,"unguessable","function hidden_guess() -> int { return 1; }\nexport;\n");
    configure(root,{"unguessable"});
    const auto generated="__strut_pkg_n"+identifier_hex("unguessable")+"_v"+identifier_hex("1.0.0")+"_c"+guessed_cache.filename().string()+"_f_"+identifier_hex("hidden_guess");
    auto guessed=check(root,"include <unguessable>;\nfunction main() -> int { return "+generated+"(); }\n",false);
    require(guessed.find("not exported by a directly included package")!=std::string::npos,"generated private identity cannot be guessed",guessed);
    configure(root,{});write(root/"project-local.p","function local_value() -> int { return 1; }\n");check(root,"include \"project-local.p\";\nfunction main() -> int { return local_value(); }\n",true);
    auto direct_cache=check(root,"include \""+(guessed_cache/"main.p").generic_string()+"\";\nfunction main() -> int { return 0; }\n",false);require(direct_cache.find("use `include <package>;` instead")!=std::string::npos,"application direct cache include rejected",direct_cache);
    std::error_code symlink_error;std::filesystem::create_symlink(guessed_cache/"main.p",root/"cached-link.p",symlink_error);if(!symlink_error){auto symlink_cache=check(root,"include \"cached-link.p\";\nfunction main() -> int { return 0; }\n",false);require(symlink_cache.find("use `include <package>;` instead")!=std::string::npos,"application symlink into cache rejected",symlink_cache);}

    cache_package(root,"package-main","function main() -> int { return 99; }\nfunction package_api() -> int { return 8; }\nexport package_api;\n");
    configure(root,{"package-main"});
    check(root,"include <package-main>;\nfunction main() -> void { println(package_api()); return; }\n",true);
    compile_and_run(root,"8\n");
    cache_package(root,"export-main","function main() -> int { return 99; }\nexport main;\n");configure(root,{"export-main"});auto exported_main=check(root,"include <export-main>;\nfunction main() -> int { return 0; }\n",false);require(exported_main.find("package main cannot be exported")!=std::string::npos,"explicit package main rejection",exported_main);

    cache_package(root,"nested-export","function hidden() -> int { export hidden; return 1; }\n");configure(root,{"nested-export"});auto nested_export=check(root,"include <nested-export>;\nfunction main() -> int { return 0; }\n",false);require(nested_export.find("only allowed at the top level")!=std::string::npos,"nested package export rejection",nested_export);
    configure(root,{});auto root_export=check(root,"function main() -> int { export main; return 0; }\n",false);require(root_export.find("only allowed at the top level")!=std::string::npos,"nested application export rejection",root_export);
    auto root_top_export=check(root,"export main;\nfunction main() -> int { return 0; }\n",false);require(root_top_export.find("only allowed in a package manifest entry")!=std::string::npos,"application export rejection",root_top_export);
    cache_package(root,"implementation-export","include \"implementation.p\";\nexport;\n",{{"implementation.p","function hidden() -> int { return 1; }\nexport hidden;\n"}});configure(root,{"implementation-export"});auto implementation_export=check(root,"include <implementation-export>;\nfunction main() -> int { return 0; }\n",false);require(implementation_export.find("only allowed in a package manifest entry")!=std::string::npos,"quoted implementation export rejection",implementation_export);

    cache_package(root,"global-aggregate","struct Item { int value; }\nItem[] items := [Item { value: 7 }];\nfunction item_value() -> int { return items[0].value; }\nexport item_value;\n");configure(root,{"global-aggregate"});check(root,"include <global-aggregate>;\nfunction main() -> void { println(item_value()); return; }\n",true);compile_and_run(root,"7\n");

    cache_package(root,"ffi-left","extern \"C\" function abs(int value) -> int;\nfunction left_abs() -> int { unsafe { return abs(-7); } }\nexport left_abs;\n");
    cache_package(root,"ffi-right","extern \"C\" function abs(int value) -> int;\nfunction right_abs() -> int { unsafe { return abs(-5); } }\nexport right_abs;\n");
    configure(root,{"ffi-left","ffi-right"});check(root,"include <ffi-left>;\ninclude <ffi-right>;\nfunction main() -> void { println(left_abs() + right_abs()); return; }\n",true);compile_and_run(root,"12\n");
    auto private_ffi=check(root,"include <ffi-left>;\nfunction main() -> int { unsafe { return abs(-1); } }\n",false);require(private_ffi.find("not exported by a directly included package")!=std::string::npos,"package extern C remains private",private_ffi);

    cache_package(root,"quoted-traversal","include \"../outside.p\";\nexport;\n");
    configure(root,{"quoted-traversal"});
    auto traversal=check(root,"include <quoted-traversal>;\nfunction main() -> int { return 0; }\n",false);
    require(traversal.find("quoted include escapes package root")!=std::string::npos,"quoted package traversal rejection",traversal);

    configure(root,{"legacy"});
    auto subpath_traversal=check(root,"include <legacy/../main.p>;\nfunction main() -> int { return 0; }\n",false);
    require(subpath_traversal.find("package subpath include escapes package root")!=std::string::npos,"package subpath traversal rejection",subpath_traversal);

    cache_package(root,"shared","function shared_api() -> int { return 5; }\nexport shared_api;\n");cache_package(root,"diamond-a","include <shared>;\nfunction a_api() -> int { return shared_api(); }\nexport a_api;\n",{}, {"shared"});cache_package(root,"diamond-b","include <shared>;\nfunction b_api() -> int { return shared_api(); }\nexport b_api;\n",{}, {"shared"});configure(root,{"diamond-a","diamond-b"});check(root,"include <diamond-a>;\ninclude <diamond-b>;\nfunction main() -> void { println(a_api() + b_api()); return; }\n",true);compile_and_run(root,"10\n");
    return 0;
}
