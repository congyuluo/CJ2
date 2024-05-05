//
// Created by congyu on 9/7/23.
//

#ifndef CJ_2_VALUE_H
#define CJ_2_VALUE_H

typedef struct Value {
    union {
        Object* obj;
        double num;
        bool boolean;
    };
    uint16_t type;
} Value;


#endif //CJ_2_VALUE_H
