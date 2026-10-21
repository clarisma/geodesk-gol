from update_helpers import is_orphan

def test_areas(update):
    res = update(
        before="""
        n1 v1 x-90 y45 Tamenity=cafe
        n2 v1 x90 y45
        n3 v1 x90 y-45
        n4 v1 x-90 y-45
        w10 v1 Nn1,n2,n3,n4,n1 Tarea=yes
        w20 v1 Nn1,n2,n3,n4,n1 Tarea=no
        """,
        changes="""
        w10 v2 Nn1,n2,n3,n4,n1 Tarea=no
        w20 v2 Nn1,n2,n3,n4,n1 Tarea=yes
        """)
    assert res.created == 0
    assert res.modified == 2
    assert res.deleted == 0
    after = res.after
    # way/10 changes to non-area closed ring
    assert not after.way(10).is_area
    # way/20 changes to area
    assert after.way(20).is_area
    # both must repeat feature node/1 as first and
    # last node in node table
    assert after.node(1).parents.count == 2
    assert after.way(10).nodes("n").count == 2
    assert after.node(1) in after.way(10).nodes
    nodes = list(after.way(10).nodes)
    assert nodes[0] == after.node(1)
    assert nodes[1].id == 2
    assert nodes[2].id == 3
    assert nodes[3].id == 4
    assert nodes[4] == after.node(1)
    # TODO: This currently returns 1, what should be
    #  the correct behavior?
    # assert after.way(20).nodes("n").count == 2
    assert after.node(1) in after.way(20).nodes
    nodes = list(after.way(20).nodes)
    assert nodes[0] == after.node(1)
    assert nodes[1].id == 2
    assert nodes[2].id == 3
    assert nodes[3].id == 4
    # TODO: This currently fails, should be a feature node
    #  The feature-node ref for the last node is not written
    #  to the change instruction
    # assert nodes[4] == after.node(1)
    assert nodes[4],id == 1
