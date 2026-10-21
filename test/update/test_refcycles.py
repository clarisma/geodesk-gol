
def test_refcycle_resolution(update):
    res = update(
        before="""
        n1 v1 x-90 y45
        n2 v1 x90 y45
        w10 v1 Nn1,n2
        r100 v1 Mw10@
        r200 v1 Mr100@
        r300 v1 Mr200@
        """,
        changes="""
        r100 v2 Mw10@,r300@remove_this_reference
        """)
    assert res.created == 0
    # TODO: Doesn't work yet, all relations are marked modified
    # assert res.modified == 1
    assert res.deleted == 0
    after = res.after
    # relation/100 should lose its member
    assert after.relation(100).members.count == 1
    assert after.relation(300) not in after.relation(100).members
    assert dict(after.relation(100).tags) == { "geodesk:removed_refcycles": 1 }
    assert after.relation(200).members.count == 1
    assert after.relation(300).members.count == 1