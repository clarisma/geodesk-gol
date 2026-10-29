from geodesk import Box

def test_implicit_geom_change(update):
    res = update(
        before="""
        n1 v1 x20 y40
        n2 v1 x30 y40
        n3 v1 x30 y35
        n4 v1 x20 y35
        w10 v1 Nn1,n2,n3,n4,n1 Tleisure=park
        w20 v1 Nn1,n2,n3,n4,n1 Tbarrier=fence
        r100 v1 Mw20@outer Tnatural=grass,type=multipolygon
        r200 v1 Mw20@outer Ttype=site
        """,
        changes="""
        n2 v1 x32 y42
        """,
    )
    assert res.created == 0
    assert res.modified == 4
        # TODO: We may change the rules about what causes a
        # relation to be logically changed
    assert res.deleted == 0
    after = res.after
    # way/10 (park) stays an area
    assert after.way(10).is_area
    # way/20 (fence) stays a linear way
    assert not after.way(20).is_area
    # relation/100 (grass) stays an area
    assert after.relation(100).is_area
    # relation/200 (site) stays a non-area
    assert not after.relation(200).is_area
    # bounds of all features are properly updated
    # so features can be found
    box = Box(w=10, e=50, n=50, s=42)
    assert after(box).count == 4
    assert after("a[leisure=park]")(box).count == 1
    assert after("r[type=site]")(box).count == 1

# Regression test for #71
def test_node_move_cascade(update):
    res = update(
        before="""
        n1 v1 x-90 y45
        n2 v1 x90 y45
        n3 v1 x90 y-45
        n4 v1 x-90 y-45
        n5 v1 x-120 y0 Tplace=city
        w10 v1 Nn1,n2,n3,n4,n1
        r100 v1 Mw10@outer,n5@admin_centre Ttype=boundary
        r200 v1 Mr100@subarea Ttype=boundary
        """,
        changes="""
        n5 v2 x-30 y0
        """,
    )
    assert res.created == 0
    assert res.modified == 3
    after = res.after
    b = Box(w=-180,n=90,s=-90,e=-100)
    assert after(b).count == 0
