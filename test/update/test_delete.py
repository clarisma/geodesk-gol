from update_helpers import is_orphan

def test_delete_relation(update):
    res = update(
        before="""
        n1 v1 x-90 y45
        n2 v1 x90 y45
        n3 v1 x90 y-45
        n4 v1 x-90 y-45
        w10 v1 Nn1,n2
        w20 v1 Nn2,n3
        r100 v1 Mw10@,w20@,n3@,n4@
        r200 v1 Mw20@
        """,
        changes="""
        r100 v1 dD
        """,
    )
    assert res.created == 0
    assert res.modified == 1
    assert res.deleted == 2
    after = res.after
    # way/1 is no longer a relation member
    assert after.way(10).parents.count == 0
    # way/20 is only member of relation/200
    assert after.way(20).parents.count == 1
    assert after.relation(200) in after.way(20).parents
    # node/3 loses feature status (deleted from tile),
    # since it is no longer a relation member (but does
    # not become an orphan, since it is part of way/20)
    assert after.node(3) is None
    # node/4 becomes an orphan
    assert is_orphan(after.node(4))

# TODO: This fails, cannot build an empty gol
def notest_delete_all(update):
    res = update(
        before="""
        n1 v1 x-90 y45
        n2 v1 x90 y45
        w10 v1 Nn1,n2
        r100 v1 Mw10@
        """,
        changes="""
        n1 v2 dD
        n2 v2 dD
        """)
    assert res.created == 0
    assert res.modified == 0
    assert res.deleted == 2
    after = res.after
    assert after.count == 0
