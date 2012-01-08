pushd src/demos

waf
dmengine --config=demo.length=550 --config=demo.demo=level_complete "--config=demo.title=Level Complete"
dmengine --config=demo.length=1000 --config=demo.demo=parallax "--config=demo.title=Parallax"
dmengine --config=demo.length=550 --config=demo.demo=main_menu "--config=demo.title=Main Menu Animation"
dmengine --config=demo.length=700 --config=demo.demo=hud "--config=demo.title=Hud"

popd

