//
// Created by congyu on 9/7/23.
//

#include "chunk.h"
#include "object.h"
#include "primitiveVars.h"

// Use this macro to define a new user function in the array
#define USER_FUNCTION(inCount, outCount, funcName, cFunction) {.in = inCount, .out = outCount, .name = funcName, .cFunc = cFunction}

// This is the definition of a user function
// Value (*cMethodType)(Value, Value*, int);

// Define functions here

Value pi(Value self, Value* args, int numArgs) {
    // Load the number of rounds from the first argument
    int rounds = VALUE_NUMBER_VALUE(args[0]);

    // Calculate pi using the Leibniz formula
    rounds += 2u;

    double pi = 1.0;
    for (unsigned i=2u; i < rounds; ++i) // use ++i instead of i++
    {
        double x = -1.0 + 2.0 * (i & 0x1); // allows vectorization
        pi += (x / (2u * i - 1u)); // double / unsigned = double
    }

    pi *= 4;

    // Wrap the result in a Value and return it
    return NUMBER_VAL(pi);
}

// This is what the VM will use to integrate with the host language
uint32_t funcCount = 1;
userFunction userFuncs[] = {USER_FUNCTION(1, 1, "C_CalculatePi", pi)};
