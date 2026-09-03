import fnmatch
import itertools

import SCons
from fbt.util import GLOB_FILE_EXCLUSION
from SCons.Node.FS import has_glob_magic
from SCons.Script import Flatten


def GlobRecursive(env, pattern, node=".", exclude=[]):
    exclude = list(set(Flatten(exclude) + GLOB_FILE_EXCLUSION))
    # print(f"Starting glob for {pattern} from {node} (exclude: {exclude})")
    results = []
    if isinstance(node, str):
        node = env.Dir(node)
    # Only initiate actual recursion if special symbols can be found in 'pattern'
    if has_glob_magic(pattern):
        for f in node.glob("*", source=True, exclude=exclude):
            if isinstance(f, SCons.Node.FS.Dir):
                results += env.GlobRecursive(pattern, f, exclude)
        results += node.glob(
            pattern,
            source=True,
            exclude=exclude,
        )
    # Otherwise, just assume that file at path exists
    else:
        results.append(node.File(pattern))
    ## Debug
    # print(f"Glob result for {pattern} from {node}: {results}")
    return results


def GatherSources(env, sources_list, node="."):
    sources_list = list(set(Flatten(sources_list)))
    include_sources = list(filter(lambda x: not x.startswith("!"), sources_list))
    exclude_sources = list(x[1:] for x in sources_list if x.startswith("!"))
    gathered_sources = list(
        itertools.chain.from_iterable(
            env.GlobRecursive(
                source_type,
                node,
                exclude=exclude_sources,
            )
            for source_type in include_sources
        )
    )
    if exclude_sources:
        filtered = []
        for src in gathered_sources:
            src_rel = src.get_path(node).replace("\\", "/")
            src_name = src.name
            excluded = False
            for pat in exclude_sources:
                pat_clean = pat.replace("\\", "/").rstrip("/")
                if (
                    fnmatch.fnmatch(src_rel, pat_clean)
                    or fnmatch.fnmatch(src_rel, f"{pat_clean}/*")
                    or fnmatch.fnmatch(src_name, pat_clean)
                ):
                    excluded = True
                    break
            if not excluded:
                filtered.append(src)
        gathered_sources = filtered
    ## Debug
    # print(
    #     f"Gathered sources for {sources_list} from {node}: {list(f.path for f in gathered_sources)}"
    # )
    return gathered_sources


def generate(env):
    env.AddMethod(GlobRecursive)
    env.AddMethod(GatherSources)


def exists(env):
    return True
