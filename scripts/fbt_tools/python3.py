def generate(env):
    py_name = "python3"
    if env["PLATFORM"] == "win32":
        # On Windows, Python 3 executable is usually just "python"
        py_name = "python.exe"

    # Find the full path to python
    python_path = env.WhereIs(py_name)
    if python_path:
        py_name = python_path

    env.SetDefault(
        PYTHON3=py_name,
    )


def exists(env):
    return True
