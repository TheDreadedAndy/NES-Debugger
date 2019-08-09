def main():
    with open("out1") as file:
        out1set = set(file.read().splitlines())

    with open("out2") as file:
        out2set = set(file.read().splitlines())

    diff = out1set.symmetric_difference(out2set)
    print("out1 - out2:", out1set - out2set)
    print("out2 - out1:", out2set - out1set)

main()
