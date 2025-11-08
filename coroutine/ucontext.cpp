#include<ucontext.h>
#include<iostream>
// ucontext 已被标记为弃用：在现代操作系统（特别是 glibc 2.8+）上已不推荐使用，部分系统可能已移除。更多应用场景可使用 boost::context、libco、协程库、C++20 协程等替代方案。
// 栈空间大小和独立性：每个协程应拥有独立且足够的栈。例如 1024 字节过小且两个协程共用同一栈会导致严重问题（数据覆盖、崩溃等）。
// 线程安全与可移植性：ucontext 本身为用户态上下文，不涉及内核调度，也不是可重入线程安全的机制。
// uc_link：当协程函数执行完毕（即返回），系统会自动切换到 uc_link。这里设置为 main_ctx，保证协程结束后返回主函数。

// ucontext.h：提供 UNIX 系统下的用户态上下文切换 API（getcontext、setcontext、makecontext、swapcontext 等）。
// ctx[3]：分别保存三个协程（fun1、func2、fun3）的执行上下文。
// main_ctx：保存主线程（main 函数）自身的上下文，用于协程结束后返回。
// count：全局计数器，在三个协程之间共享，用于输出与结束条件。
ucontext_t ctx[3];
ucontext_t main_ctx;

int count =0;

// while (count < 30)：只要全局计数器未达到 30，就持续执行。
// count++：递增共享计数器。
// 输出当前协程与计数。
// swapcontext(&ctx[0], &main_ctx)：
// 将当前上下文（包括寄存器、栈指针等）保存到 ctx[0]。
// 切换到 main_ctx，即恢复 main 函数的上下文，返回 main 中继续执行。
// func2、fun3 实现完全类似，只是使用 ctx[1]、ctx[2]。
// 协程函数内部并未显式结束，当 count >= 30 时会跳出循环，自然返回到 uc_link 指定的上下文（即 main_ctx）。
void fun1(){
    while(count < 30){
        count++;
        std::cout << "fun1 count: " << count << std::endl;
        swapcontext(&ctx[0], &main_ctx);
    }
}

void func2(){
    while(count < 30){
        count++;
        std::cout << "func2 count: " << count << std::endl;
        swapcontext(&ctx[1], &main_ctx);
    }
}

void fun3(){
    while(count < 30){
        count++;
        std::cout << "fun3 count: " << count << std::endl;
        swapcontext(&ctx[2], &main_ctx);
    }
}


int main(){
//     getcontext(&ctx[0])：初始获取当前上下文，作为 ctx[0] 的基础。
// 设置 ctx[0] 的栈区域为 stack1，大小 1024 字节。
// uc_link = &main_ctx：当 fun1 函数返回后自动切换到 main_ctx。
// makecontext(&ctx[0], fun1, 0)：指定当 ctx[0] 被恢复执行时，从 fun1 入口开始，无参数。
    char stack1[1024];
    char stack2[1024];
    char stack3[1024];
    
    getcontext(&ctx[0]);
    ctx[0].uc_stack.ss_sp = stack1;
    ctx[0].uc_stack.ss_size = sizeof(stack1);
    ctx[0].uc_link = &main_ctx;
    makecontext(&ctx[0], fun1, 0);

    getcontext(&ctx[1]);
    ctx[1].uc_stack.ss_sp = stack2;
    ctx[1].uc_stack.ss_size = sizeof(stack2);
    ctx[1].uc_link = &main_ctx;
    makecontext(&ctx[1], func2, 0);

    getcontext(&ctx[2]);
    ctx[2].uc_stack.ss_sp = stack3;
    ctx[2].uc_stack.ss_size = sizeof(stack3);
    ctx[2].uc_link = &main_ctx;
    makecontext(&ctx[2], fun3, 0);

    std::cout<<"swap context" <<std::endl;
    while(count <= 30){
        swapcontext(&main_ctx, &ctx[count % 3]);
    }
    return 0;
}