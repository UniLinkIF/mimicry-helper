// mimicry-helper: a thin CLI driver over the `mimicry` library (GPL-3.0, github.com/Baltram/rmtools),
// built as a strictly separate out-of-process helper — RisenLab's own crate stays MIT-ish and only
// shells out to this binary, never links it. See RisenLab/docs/formats/content-layer.md for why.
//
// This file is original code written for RisenLab, but because it links the GPL mimicry library
// into a single executable, the resulting binary is a GPL-3.0 derivative work (see ../LICENSE.txt).
#include "../vendor/mimicry-source/Mimicry.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

// Same basename as outPath (an .obj path) with a .mtl extension instead — this is where
// mCObjWriter's real per-material texture references (see WriteMaterial in mi_objwriter.cpp:
// map_Kd/map_bump pull each material's actual referenced texture file name straight out of the
// parsed mesh/actor scene) land, so RisenLab can read it back and auto-match a real texture
// instead of asking the user to hunt for one by hand.
std::string MtlPathFor(mCString const& outPath) {
    std::string s(outPath.GetText());
    size_t dot = s.find_last_of('.');
    return (dot != std::string::npos ? s.substr(0, dot) : s) + ".mtl";
}

bool ConvertMeshToObj(mCString const& inPath, mCString const& outPath) {
    mCFileStream in(inPath, mEFileOpenMode_Read);
    if (!in.IsOpen()) {
        std::fprintf(stderr, "could not open %s\n", inPath.GetText());
        return false;
    }
    mCScene scene;
    mCXmshReader::SOptions readOpts;
    if (mCXmshReader::ReadXmshFileData(scene, in, readOpts) != mEResult_Ok) {
        std::fprintf(stderr, "failed to parse xmsh: %s\n", inPath.GetText());
        return false;
    }
    mCFileStream out(outPath, mEFileOpenMode_Write);
    mCObjWriter::SOptions writeOpts;
    writeOpts.m_bWriteMtlFile = true;
    writeOpts.m_strMtlFilePath = MtlPathFor(outPath).c_str();
    if (mCObjWriter::WriteObjFileData(scene, out, writeOpts) != mEResult_Ok) {
        std::fprintf(stderr, "failed to write obj: %s\n", outPath.GetText());
        return false;
    }
    return true;
}

bool ConvertObjToMesh(mCString const& inPath, mCString const& outPath) {
    mCFileStream in(inPath, mEFileOpenMode_Read);
    if (!in.IsOpen()) {
        std::fprintf(stderr, "could not open %s\n", inPath.GetText());
        return false;
    }
    mCScene scene;
    if (mCObjReader::ReadObjFileData(scene, in) != mEResult_Ok) {
        std::fprintf(stderr, "failed to parse obj: %s\n", inPath.GetText());
        return false;
    }
    mCFileStream out(outPath, mEFileOpenMode_Write);
    mCXmshWriter::SOptions writeOpts;
    if (mCXmshWriter::WriteXmshFileData(scene, out, writeOpts) != mEResult_Ok) {
        std::fprintf(stderr, "failed to write xmsh: %s\n", outPath.GetText());
        return false;
    }
    return true;
}

bool ConvertActorToObj(mCString const& inPath, mCString const& outPath) {
    mCFileStream in(inPath, mEFileOpenMode_Read);
    if (!in.IsOpen()) {
        std::fprintf(stderr, "could not open %s\n", inPath.GetText());
        return false;
    }
    mCScene scene;
    mCXmacReader::SOptions readOpts;
    if (mCXmacReader::ReadXmacFileData(scene, in, readOpts) != mEResult_Ok) {
        std::fprintf(stderr, "failed to parse xmac: %s\n", inPath.GetText());
        return false;
    }
    mCFileStream out(outPath, mEFileOpenMode_Write);
    mCObjWriter::SOptions writeOpts;
    writeOpts.m_bWriteMtlFile = true;
    writeOpts.m_strMtlFilePath = MtlPathFor(outPath).c_str();
    if (mCObjWriter::WriteObjFileData(scene, out, writeOpts) != mEResult_Ok) {
        std::fprintf(stderr, "failed to write obj: %s\n", outPath.GetText());
        return false;
    }
    return true;
}

bool DumpMaterial(mCString const& inPath, mCString const& outPath) {
    mCFileStream in(inPath, mEFileOpenMode_Read);
    if (!in.IsOpen()) {
        std::fprintf(stderr, "could not open %s\n", inPath.GetText());
        return false;
    }
    mCGenomeMaterial material(in);
    if (!material.IsValid()) {
        std::fprintf(stderr, "failed to parse xmat: %s\n", inPath.GetText());
        return false;
    }

    mCFileStream out(outPath, mEFileOpenMode_Write);
    MIUInt elementCount = material.GetShaderElementCount();
    for (MIUInt e = 0; e < elementCount; ++e) {
        mCString elementLine("[shader_element] ");
        elementLine += material.GetShaderElementType(e);
        elementLine += "\n";
        out.Write(elementLine);

        MIUInt propCount = material.GetPropertyCount(e);
        for (MIUInt p = 0; p < propCount; ++p) {
            mCGenomeMaterial::SProperty const& prop = material.GetProperty(e, p);
            mCString line("  ");
            line += prop.m_strName;
            line += " : ";
            line += prop.m_strType;
            line += "\n";
            out.Write(line);
        }
    }
    return true;
}

void PrintUsage() {
    std::fprintf(stderr,
        "mimicry-helper commands:\n"
        "  mesh-to-obj <in.xmsh> <out.obj>\n"
        "  obj-to-mesh <in.obj> <out.xmsh>\n"
        "  material-dump <in.xmat> <out.txt>\n"
        "  actor-to-obj <in.xmac> <out.obj>\n");
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 4) {
        PrintUsage();
        return 1;
    }
    mCString cmd(argv[1]);
    mCString inPath(argv[2]);
    mCString outPath(argv[3]);

    bool ok = false;
    if (cmd == mCString("mesh-to-obj")) {
        ok = ConvertMeshToObj(inPath, outPath);
    } else if (cmd == mCString("obj-to-mesh")) {
        ok = ConvertObjToMesh(inPath, outPath);
    } else if (cmd == mCString("material-dump")) {
        ok = DumpMaterial(inPath, outPath);
    } else if (cmd == mCString("actor-to-obj")) {
        ok = ConvertActorToObj(inPath, outPath);
    } else {
        PrintUsage();
        return 1;
    }

    if (!ok) {
        return 1;
    }
    std::printf("Wrote %s\n", outPath.GetText());
    return 0;
}
