from update_helpers import is_duplicate

def test_no_changes(update):
    res = update(
        before="""
        n1 v1 x-90 y45
        n2 v1 x90 y45
        n3 v1 x90 y-45 Tbarrier=gate
        w10 v1 Nn1,n2 Thighway=primary
        """,
        changes="""
        n1 v2 x-90 y45
        n3 v2 x90 y-45 Tbarrier=gate
        w20 v1 Nn2,n3
        w30 v1 Nn9,n9
        r100 v1 Mw20@
        w20 v2 dD
        """)
    # Nothing changed:
    # - n1 has the same location
    # - n3 has the same location and tags
    # - w20 is created and then deleted
    # - r100 only has w20 as member, hence it is empty
    #   and will be disregarded
    assert res.created == 0
    assert res.modified == 0
    assert res.deleted == 0
    after = res.after
    assert after.count == 2
    assert dict(after.node(3).tags) == { "barrier": "gate" }
