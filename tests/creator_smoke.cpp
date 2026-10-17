#include "game/Scenes.h"
#include <cstdio>
using namespace ogd;
static void frames(Scene& s, int n) { for (int i = 0; i < n; ++i) { s.update(1.f / 60); s.draw(); } }
static void tap(Scene& s, float x, float y) { s.onDown(x, y); frames(s, 2); s.onUp(x, y); frames(s, 2); }
int main() {
    if (!E().init("/tmp/emptyres", "/tmp/smoke_save.json")) { puts("init failed"); return 1; }
    auto& L = E().save.customLevels;
    L.clear();
    { auto m = makeMenuScene(); frames(*m, 30); tap(*m, 641, 383); tap(*m, 519, 101); tap(*m, 640, 101); tap(*m, 761, 101); }
    { auto c = makeCreatorScene(); frames(*c, 10); tap(*c, 500, 507); tap(*c, 780, 507); tap(*c, 500, 213); tap(*c, 780, 213); tap(*c, 52, 670); }
    { auto m = makeMyLevelsScene(); frames(*m, 10); tap(*m, 1215, 653); }
    printf("levels after New: %zu editing=%d\n", L.size(), E().save.editingLevel);
    { auto e = makeEditLevelScene(true); frames(*e, 10); tap(*e, 554, 270); /*NO*/ tap(*e, 640, 653); e->onText("My test"); tap(*e, 640, 507); e->onText("desc here"); tap(*e, 300, 300);
      tap(*e, 427, 304); /*edit*/ }
    printf("name=[%s] desc=[%s]\n", L[0].name.c_str(), L[0].desc.c_str());
    { auto ed = makeEditorScene(); frames(*ed, 10);
      // build: place a few blocks from page 1
      tap(*ed, 414, 153); tap(*ed, 300, 250); tap(*ed, 364, 250); tap(*ed, 428, 250);
      tap(*ed, 500, 153); tap(*ed, 500, 330);
      // next page, pick portal, place
      tap(*ed, 917, 108); tap(*ed, 500, 153); tap(*ed, 700, 300);
      tap(*ed, 917, 108); tap(*ed, 917, 108); tap(*ed, 917, 108); // wrap to page 1
      // edit tab: select and move
      tap(*ed, 90, 104); tap(*ed, 300, 250); tap(*ed, 414, 153); tap(*ed, 862, 63);
      // swipe select + drag
      tap(*ed, 1132, 149);
      ed->onDown(250, 600); ed->onMove(600, 300); ed->onMove(900, 230); ed->onUp(900, 230); frames(*ed, 3);
      tap(*ed, 414, 153); tap(*ed, 1132, 52);
      // delete tab
      tap(*ed, 90, 39); tap(*ed, 300, 250); tap(*ed, 540, 130);
      // undo/redo, zoom, slider, settings, pause
      tap(*ed, 54, 675); tap(*ed, 161, 675); tap(*ed, 1227, 149); tap(*ed, 1227, 52);
      tap(*ed, 640, 675); ed->onDown(500, 675); ed->onMove(700, 675); ed->onUp(700, 675);
      tap(*ed, 1129, 675); tap(*ed, 898, 180); tap(*ed, 489, 383); tap(*ed, 500, 470); tap(*ed, 640, 90); tap(*ed, 640, 90);
      tap(*ed, 1236, 675); tap(*ed, 640, 406); }
    printf("saved data: %s\n", L[0].data.c_str());
    { auto ed = makeEditorScene(); frames(*ed, 5); tap(*ed, 1236, 675); tap(*ed, 640, 217); tap(*ed, 709, 270); }
    L.clear();
    puts("smoke ok");
    return 0;
}
