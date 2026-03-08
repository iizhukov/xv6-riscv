.section .text
.globl main

main:
    addi sp, sp, -64
    mv s0, sp

    mv a0, s0
    li a1, 64
    call gets
    
    mv t0, s0

    li t2, 10                # 10 = '\n'
remove_newline_symb:
    lb t1, 0(t0)
    beq t1, t2, found_newline_symb
    beqz t1, 3f
    addi t0, t0, 1
    j remove_newline_symb

found_newline_symb:
    sb zero, 0(t0)
3:

    la a0, fmt_msg
    mv a1, s0
    call printf
    
    lb t0, 0(s0)
    beqz t0, error
    
    mv t0, s0

    li t2, 32               # ' ' = 32
1:
    lb t1, 0(t0)
    beqz t1, error
    beq t1, t2, 2f
    addi t0, t0, 1
    j 1b
    
2:
    sb zero, 0(t0)

    mv a0, s0
    call atoi
    mv s1, a0
    
    addi a0, t0, 1
    call atoi
    mv s2, a0
    
    mv a0, s1
    mv a1, s2
    call add
    
    mv a3, a0 
    mv a1, s1
    mv a2, s2
    la a0, result_msg
    call printf
    
    addi sp, sp, 64

    li a0, 0
    call exit

error:
    li a0, 2
    la a1, error_msg
    li a2, 6
    call write

    addi sp, sp, 64

    li a0, 1
    call exit

.section .rodata
fmt_msg:
    .string "|%s|\n"
result_msg:
    .string "add(%d, %d) = %d\n"
error_msg:
    .string "ERROR\n"