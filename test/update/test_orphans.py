
def test_orphans_and_tags(update):
    """
    Test promotion/demotion of nodes that gain/lose tags
    """
    res = update(
        before="""
        n1 v1 x-90 y45
        n2 v1 x90 y45 Tbarrier=gate
        n3 v1 x90 y-45
        n4 v1 x-90 y-45
        n5 v1 x-120 y-60
        n6 v1 x120 y-60 Tamenity=restaurant
        w10 v1 Nn1,n2 Thighway=residential,name=Main%20%Street
        w20 v1 Nn2,n3 Thighway=primary,name=Prospect%20%Street
        """,
        changes="""
        n2 v2 x90 y45
        n3 v2 x90 y-45 Thighway=crossing
        n5 v2 x-120 y-60 Tshop=bakery
        n6 v2 x120 y-60
        w10 v2 Nn1,n2 Thighway=residential
        """)
    after = res.after
    # node/2 turns anonymous (It loses its tags; but no orphan,
    # since it's part of way/1)
    assert after.node(2) is None
    # node/3 becomes a feature (It gains tags)
    assert dict(after.node(3).tags) == { "highway": "crossing" }
    # node/5 loses orphan status (it now has tags)
    assert dict(after.node(5).tags) == { "shop": "bakery" }
    # node/6 becomes an orphan (it loses all its tags)
    assert dict(after.node(6).tags) == { "geodesk:orphan": "yes" }
    # way/10 changes tags (loses its name)
    assert dict(after.way(10).tags) == { "highway": "residential" }

def test_orphans_and_relations(update):
    """
    Tet promotion/demotion of nodes that are added to/
    removed from relations
    """
    res = update(
        before="""
        n1 v1 x-90 y45
        n2 v1 x90 y45
        n3 v1 x90 y-45
        n4 v1 x-90 y-45
        n5 v1 x-90 y0
        n6 v1 x-45 y0
        w10 v1 Nn1,n2,n3,n4,n5
        r100 v1 Mn4@,n5@
        """,
        changes="""
        w10 v2 Nn1,n2
        r100 v2 Mn4@,n6@
        r200 v1 Mn3@
        """)
    after = res.after
    # node/4 stays unchanged (dropped by way/10,
    # but retained by relation/100)
    assert dict(after.node(4).tags) == {}
    assert after.node(4).parents.count == 1
    assert after.relation(100) in after.node(4).parents
    # Node 3 is promoted from way-node to relation member
    assert dict(after.node(3).tags) == {}
    assert after.node(3).parents.count == 1
    assert after.relation(200) in after.node(3).parents
    # Node 5 becomes an orphan
    assert dict(after.node(5).tags) == { "geodesk:orphan": "yes" }
    assert after.node(5).parents.count == 0
    # Node 6 is no longer an orphan
    assert dict(after.node(6).tags) == {}
    assert after.node(6).parents.count == 1
    assert after.relation(100) in after.node(6).parents