#include<setjmp.h>
#include<iostream>

jmp_buf env;

void fun(int arg){
    std::cout << "In fun, arg: " << arg << std::endl;
    longjmp(env, ++arg);
}

int main(){
    int ret = setjmp(env);
    if(ret == 0){
        fun(ret);
    }else if(ret == 1){
        fun(ret);
    }else if(ret == 2){
        fun(ret);
    }
    return 0;
}

