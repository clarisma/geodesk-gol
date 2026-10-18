from functools import partial
import pytest

from update_helpers import run_update_case

@pytest.fixture
def update(gol_tool, tmp_path):
    return partial(run_update_case, gol_tool, tmp_path)