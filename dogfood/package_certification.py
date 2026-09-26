#!/usr/bin/env python3
"""End-to-end certification for locked, reproducible Strut package graphs."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(args, cwd, env=None, expect=0):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != expect:
        raise RuntimeError(f"{args} returned {result.returncode}, expected {expect}\n{result.stdout}\n{result.stderr}")
    return result


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def commit(repo, message):
    run(["git", "add", "."], repo)
    run(["git", "commit", "--quiet", "-m", message], repo)
    return run(["git", "rev-parse", "HEAD"], repo).stdout.strip()


def make_repo(base, name, manifest, source):
    repo = base / name
    repo.mkdir()
    run(["git", "init", "--quiet"], repo)
    run(["git", "config", "user.email", "packages@strut.invalid"], repo)
    run(["git", "config", "user.name", "Strut Package Certification"], repo)
    write_json(repo / "strut.json", manifest)
    (repo / "main.p").write_text(source, encoding="utf-8")
    return repo, commit(repo, f"{name} 1.0.0")


def locked_cache_path(home, package):
    return home / "cache" / "packages" / package["name"] / package["version"] / package["checksum"][7:]


def executable(path):
    return path.with_suffix(".exe") if os.name == "nt" else path


def main():
    compiler = Path(sys.argv[1] if len(sys.argv) > 1 else "build/strut").resolve()
    with tempfile.TemporaryDirectory(prefix="strut package certification ") as temporary:
        root = Path(temporary)
        remotes = root / "remote repositories"
        remotes.mkdir()
        common, common_rev = make_repo(remotes, "common", {
            "name": "common", "version": "1.0.0", "entry": "main.p"
        }, "function common_value() -> int { return 20; }\n")
        dependency = lambda repo, rev: {"version": "^1.0.0", "git": str(repo), "rev": rev}
        package_a, a_rev = make_repo(remotes, "package-a", {
            "name": "package-a", "version": "1.0.0", "entry": "main.p",
            "dependencies": {"common": dependency(common, common_rev)}
        }, "include <common>;\nfunction package_a_value() -> int { return common_value() + 1; }\n")
        package_b, b_rev = make_repo(remotes, "package-b", {
            "name": "package-b", "version": "1.0.0", "entry": "main.p",
            "dependencies": {"common": dependency(common, common_rev)}
        }, "include <common>;\nfunction package_b_value() -> int { return common_value() + 1; }\n")

        official_root = root / "official repositories"
        official_root.mkdir()
        sqlite, _ = make_repo(official_root, "sqlite", {
            "name": "sqlite", "version": "1.0.0", "entry": "main.p"
        }, "function sqlite_value() -> int { return 22; }\n")
        run(["git", "tag", "v1.0.0"], sqlite)
        official_project = root / "official shorthand"
        official_project.mkdir()
        write_json(official_project / "strut.json", {
            "name": "official-app", "version": "0.1.0", "entry": "main.p", "dependencies": {}
        })
        (official_project / "main.p").write_text(
            "include <sqlite>;\nfunction main() -> int { print(sqlite_value()); return 0; }\n", encoding="utf-8")
        official_env = os.environ.copy()
        official_env["STRUT_HOME"] = str(root / "official home")
        official_env["STRUT_OFFICIAL_PACKAGE_BASE"] = str(official_root)
        install = run([compiler, "install", "sqlite@^1.0.0"], official_project, official_env)
        assert "installed official package sqlite 1.0.0" in install.stdout
        official_manifest = json.loads((official_project / "strut.json").read_text(encoding="utf-8"))
        assert official_manifest["dependencies"] == {"sqlite": "^1.0.0"}
        official_lock = json.loads((official_project / "strut.lock.json").read_text(encoding="utf-8"))
        assert official_lock["packages"][0]["source_kind"] == "official"
        official_packages = json.loads(run([compiler, "packages", "--json"], official_project, official_env).stdout)
        assert official_packages["packages"][0]["official"] is True
        official_app = executable(official_project / "app")
        run([compiler, "main.p", "-o", official_app], official_project, official_env)
        assert run([official_app], official_project, official_env).stdout.strip() == "22"

        seed = root / "seed project"
        seed.mkdir()
        manifest = {"name": "package-app", "version": "0.1.0", "entry": "main.p", "dependencies": {
            "package-a": dependency(package_a, a_rev), "package-b": dependency(package_b, b_rev)}}
        write_json(seed / "strut.json", manifest)
        (seed / "main.p").write_text("include <package-a>;\ninclude <package-b>;\nfunction main() -> int { print(package_a_value() + package_b_value()); return 0; }\n", encoding="utf-8")
        seed_home = root / "seed home"
        seed_env = os.environ.copy(); seed_env["STRUT_HOME"] = str(seed_home)
        run([compiler, "update"], seed, seed_env)
        original_lock = (seed / "strut.lock.json").read_bytes()
        original_graph = json.loads(original_lock)
        assert [p["name"] for p in original_graph["packages"]] == ["common", "package-a", "package-b"]
        assert sum(1 for p in original_graph["packages"] if p["name"] == "common") == 1
        assert next(p for p in original_graph["packages"] if p["name"] == "common")["direct"] is False

        outputs = []
        reproductions = []
        for number in (1, 2):
            project = root / f"clean reproduction {number}"
            project.mkdir()
            shutil.copy2(seed / "strut.json", project / "strut.json")
            shutil.copy2(seed / "strut.lock.json", project / "strut.lock.json")
            shutil.copy2(seed / "main.p", project / "main.p")
            home = root / f"clean home {number}"
            env = os.environ.copy(); env["STRUT_HOME"] = str(home)
            run([compiler, "install"], project, env)
            assert (project / "strut.lock.json").read_bytes() == original_lock
            package_json = json.loads(run([compiler, "packages", "--json"], project, env).stdout)
            project_json = json.loads(run([compiler, "project", "--json"], project, env).stdout)
            assert package_json["offline_available"] and len(package_json["packages"]) == 3
            assert project_json["graph_fully_resolved"] and project_json["offline_available"]
            app = executable(project / "app")
            run([compiler, "main.p", "-o", app], project, env)
            output = run([app], project, env).stdout.strip()
            assert output == "42"
            outputs.append(output); reproductions.append((project, home, env))
        assert outputs[0] == outputs[1] and (reproductions[0][0] / "strut.lock.json").read_bytes() == (reproductions[1][0] / "strut.lock.json").read_bytes()

        project, home, env = reproductions[0]
        offline_env = env.copy(); offline_env["PATH"] = str(root / "network sentinel with no git")
        run([compiler, "install", "--offline"], project, offline_env)
        assert run([executable(project / "app")], project, env).stdout.strip() == "42"

        common_lock = next(p for p in original_graph["packages"] if p["name"] == "common")
        common_cache = locked_cache_path(home, common_lock)
        shutil.rmtree(common_cache)
        missing = run([compiler, "install", "--offline"], project, offline_env, expect=1)
        assert "common" in missing.stderr and common_lock["version"] in missing.stderr and common_lock["revision"] in missing.stderr
        assert "run `strut install`" in missing.stderr
        run([compiler, "install"], project, env)

        (common_cache / "main.p").write_text("corrupt package contents\n", encoding="utf-8")
        corrupt = run([compiler, "install", "--offline"], project, offline_env, expect=1)
        assert "missing verified package 'common'" in corrupt.stderr
        run([compiler, "install"], project, env)
        assert hashlib.sha256((common_cache / "main.p").read_bytes()).hexdigest() != hashlib.sha256(b"corrupt package contents\n").hexdigest()
        (common_cache / "strut.json").write_text("{}\n", encoding="utf-8")
        run([compiler, "install", "--offline"], project, offline_env, expect=1)
        run([compiler, "install"], project, env)

        interrupted = home / "cache" / "packages" / ".staging" / "interrupted-install"
        interrupted.mkdir(parents=True); (interrupted / "partial").write_text("partial", encoding="utf-8")
        run([compiler, "install"], project, env)
        assert not (home / "cache" / "packages" / "common" / "1.0.0" / "partial").exists()

        concurrent_home = root / "concurrent home"
        concurrent_env = os.environ.copy(); concurrent_env["STRUT_HOME"] = str(concurrent_home)
        processes = [subprocess.Popen([str(compiler), "install"], cwd=p, env=concurrent_env,
                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) for p, _, _ in reproductions]
        results = [process.communicate() + (process.returncode,) for process in processes]
        if any(code != 0 for _, _, code in results):
            raise RuntimeError(f"concurrent install failed: {results}")
        for package in original_graph["packages"]:
            path = locked_cache_path(concurrent_home, package)
            assert path.is_dir() and (path / "strut.json").is_file()
            siblings = [p for p in path.parent.iterdir() if p.is_dir()]
            assert len(siblings) == 1
        assert all((p / "strut.lock.json").read_bytes() == original_lock for p, _, _ in reproductions)

        old_lock = (project / "strut.lock.json").read_bytes()
        write_json(common / "strut.json", {"name": "common", "version": "1.1.0", "entry": "main.p"})
        (common / "main.p").write_text("function common_value() -> int { return 30; }\n", encoding="utf-8")
        common_rev_2 = commit(common, "common 1.1.0")
        new_direct = []
        for repo, name, function_name in ((package_a, "package-a", "package_a_value"), (package_b, "package-b", "package_b_value")):
            write_json(repo / "strut.json", {"name": name, "version": "1.1.0", "entry": "main.p",
                       "dependencies": {"common": dependency(common, common_rev_2)}})
            (repo / "main.p").write_text(f"include <common>;\nfunction {function_name}() -> int {{ return common_value() + 1; }}\n", encoding="utf-8")
            new_direct.append(commit(repo, f"{name} 1.1.0"))
        manifest["dependencies"]["package-a"] = dependency(package_a, new_direct[0])
        manifest["dependencies"]["package-b"] = dependency(package_b, new_direct[1])
        write_json(project / "strut.json", manifest)
        run([compiler, "install"], project, env)
        assert (project / "strut.lock.json").read_bytes() == old_lock
        run([compiler, "update"], project, env)
        updated_lock = (project / "strut.lock.json").read_bytes()
        assert updated_lock != old_lock
        updated = json.loads(updated_lock)
        assert {p["version"] for p in updated["packages"]} == {"1.1.0"}
        run([compiler, "install", "--offline"], project, offline_env)
        updated_app = executable(project / "updated-app")
        run([compiler, "main.p", "-o", updated_app], project, env)
        assert run([updated_app], project, env).stdout.strip() == "62"

        print("package certification: diamond graph reproduced twice, offline sentinel passed, corruption repaired, concurrency and update certified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
