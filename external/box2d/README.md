

# How to update

* Download box2d.tar.gz from https://github.com/erincatto/box2d/releases

Extract it:
`tar xvf ~/work/defold/share/ext/box2d/box2d.tar.gz`

Copy it:
```bash
mv box2d-28adacf82377d4113f2ed00586141463244b9d10/include Box2D/include
mv box2d-28adacf82377d4113f2ed00586141463244b9d10/src Box2D/src
```

Patch it:
`(cd Box2D && patch --binary -p1 < ../patch_28adacf82377d4113f2ed00586141463244b9d10)`

