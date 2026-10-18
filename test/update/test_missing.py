from update_helpers import is_orphan

# TODO: We can't test this yet, because gol build and
#  discards ways with missing nodes, while gol update
#  attempts to fix the way, resulting in a discrepancy
def notest_missing_nodes(update):
    res = update(
        before="""
        n1 v1 x-180 y60
        n2 v1 x-30 y60
        n3 v1 x170 y60
        n4 v1 x-180 y30
        n5 v1 x-30 y30
        n6 v1 x170 y30
        n7 v1 x-180 y0
        n8 v1 x-30 y0
        n9 v1 x170 y0
        w10 v1 Nn1,n2,n3
        w20 v1 Nn4,n5,n6 Tname=Prospect%20%Street,highway=residential,maxspeed=50
        w30 v1 Nn7,n8,n9
        r100 v1 Mw10@
        r200 v1 Mw20@
        r300 v1 Mw30@
        """,
        changes="""
        w10 v2 Nn1,n555,n3
        w20 v2 Nn4,n555,n666 Tmaxspeed=50,highway=residential,name=Prospect%20%Street
        w30 v2 Nn777,n555,n666
        n2 v2 x90 y-45
        """)
    after = res.after
    assert is_orphan(after.node(2))
    assert is_orphan(after.node(5))
    assert is_orphan(after.node(6))
    assert is_orphan(after.node(7))
    assert is_orphan(after.node(8))
    assert is_orphan(after.node(9))
    assert after.way(10).nodes.count == 2
    assert dict(after.way(10).tags) == { "geodesk:missing_nodes": 1 }
    assert after.way(20).nodes.count == 2
    assert dict(after.way(20).tags) == { "geodesk:missing_nodes": 2 }
    assert after.way(30) is None
    assert after.relation(300) is None
