from update_helpers import is_duplicate

def test_duplicate_nodes(update):
    res = update(
        before="""
        n1 v1 x-90 y45
        n2 v1 x90 y45
        n3 v1 x90 y-45 Tamenity=cafe
        n4 v1 x-90 y-45
        w10 v1 Nn1,n2,n3,n4,n1 Tarea=yes
        """,
        changes="""
        n5 v1 x-90 y45 Tshop=bakery
        n6 v1 x90 y-45
        w10 v2 Nn5,n2,n6,n4,n5 Tarea=yes
        """)
    assert res.created == 3
    assert res.modified == 1
    assert res.deleted == 0
    after = res.after
    # node/1 becomes a duplicate orphan
    assert dict(after.node(1).tags) == {
        "geodesk:duplicate": "yes",
        "geodesk:orphan": "yes" }
    # TODO: assert node/5 has shared_node flag
    assert is_duplicate(after.node(6))
    # TODO: assert node/3 has shared_node flag

def test_duplicate_orphans(update):
    # lamp post with surveillance camera attached to it
    res = update(
        before="""
        n1 v1 x-90 y45
        n2 v1 x-90 y45
        n3 v1 x90 y45 Thighway=street_lamp
        n4 v1 x90 y45 Tman_made=surveillance
        n5 v1 x90 y-45 Tnatural=tree
        """,
        changes="""
        n2 v1 x-120 y45
        n4 v1 x90 y-45 Tman_made=surveillance
        n5 v1 x90 y-45
        """)
    assert res.created == 0
    assert res.modified == 4
    assert res.deleted == 0
    after = res.after
    # node/1 remains an orphan (but no longer duplicate)
    assert dict(after.node(1).tags) == { "geodesk:orphan": "yes" }
    # node/2 remains an orphan (but no longer duplicate)
    assert dict(after.node(2).tags) == { "geodesk:orphan": "yes" }
    # TODO: node/3 should lose shared_node status
    # TODO: node/4 should remain in shared_node status
    #  (moved from one coincident location to another)
    # node/5 becomes a duplicate orphan
    # (it loses its tags and becomes coincident with node/4)
    assert dict(after.node(5).tags) == {
        "geodesk:duplicate": "yes", "geodesk:orphan": "yes" }


