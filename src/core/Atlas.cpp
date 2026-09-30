#include "core/Atlas.h"
#include "core/Plist.h"

namespace ogd {

Sheet parseSheet(const std::string& xml) {
    Sheet out;
    PValue root = parsePlist(xml);
    if (!root.isDict()) return out;
    const PValue* frames = root.get("frames");
    if (!frames || !frames->isDict()) return out;

    int format = 0;
    if (const PValue* meta = root.get("metadata")) {
        if (const PValue* f = meta->get("format")) format = (int)f->num();
        if (const PValue* t = meta->get("textureFileName")) out.textureFile = t->str();
        if (out.textureFile.empty())
            if (const PValue* t = meta->get("realTextureFileName")) out.textureFile = t->str();
    }

    for (auto& kv : frames->dict) {
        const PValue& d = kv.second;
        if (!d.isDict()) continue;
        SheetFrame f;
        f.name = kv.first;

        auto numsOf = [&](const char* key) {
            const PValue* v = d.get(key);
            return v ? parseNumbers(v->str()) : std::vector<double>{};
        };

        if (format == 1 || d.get("x")) {
            auto n = [&](const char* k) { const PValue* v = d.get(k); return v ? (float)v->num() : 0.f; };
            f.x = n("x"); f.y = n("y"); f.w = n("width"); f.h = n("height");
            f.offX = n("offsetX"); f.offY = n("offsetY");
            f.srcW = n("originalWidth"); f.srcH = n("originalHeight");
            if (f.srcH < 0) f.srcH = -f.srcH;
        } else {
            std::vector<double> r = numsOf(format >= 3 ? "textureRect" : "frame");
            std::vector<double> off = numsOf(format >= 3 ? "spriteOffset" : "offset");
            std::vector<double> src = numsOf(format >= 3 ? "spriteSourceSize" : "sourceSize");
            if (r.size() < 4) continue;
            f.x = (float)r[0]; f.y = (float)r[1]; f.w = (float)r[2]; f.h = (float)r[3];
            if (off.size() >= 2) { f.offX = (float)off[0]; f.offY = (float)off[1]; }
            if (src.size() >= 2) { f.srcW = (float)src[0]; f.srcH = (float)src[1]; }
            const PValue* rot = d.get(format >= 3 ? "textureRotated" : "rotated");
            f.rotated = rot && rot->boolean();
        }
        if (f.srcW <= 0) f.srcW = f.w;
        if (f.srcH <= 0) f.srcH = f.h;
        out.frames.push_back(std::move(f));
    }
    out.ok = !out.frames.empty();
    return out;
}

} // namespace ogd
