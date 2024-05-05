from time import time

def main():

    n = 100000000
    a = time()
    pi = 4 * sum(1 / i for i in range(1 - 2*n, 2*n + 1, 4))
    
    print(f"time used {time()-a}")
    print("{:.16f}".format(pi))

main()
