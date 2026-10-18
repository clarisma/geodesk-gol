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
    after = res.after
    # node/1 becomes a duplicate orphan
    assert dict(after.node(1).tags) == {
        "geodesk:duplicate": "yes",
        "geodesk:orphan": "yes" }
    # TODO: assert node/5 has shared_node flag
    assert is_duplicate(after.node(6))
    # TODO: assert node/3 has shared_node flag
