# hello_world.py
def greet(name):
    """
    This function greets the given name.
    """
    message = f"Hello, {name}!"
    print(message)
    # A quick loop
    for i in range(3):
        print(i)


if __name__ == "__main__":
    greet("World")
