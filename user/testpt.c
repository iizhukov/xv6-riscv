#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define PGSIZE 4096

int
main(void)
{
  printf("\nТест 1: Глобальная переменная\n");
  static int global_var = 42;
  printf("Адрес глобальной переменной: 0x%lx\n", (uint64)&global_var);
  pagetable_print();

  printf("\nТест 2: Локальная переменная на стеке\n");
  int stack_var = 100;
  printf("Адрес переменной на стеке: 0x%lx\n", (uint64)&stack_var);
  pagetable_print();

  printf("\nТест 3: Выделение памяти в куче\n");
  int *heap_buf = malloc(10000);
  if (heap_buf == 0) {
    printf("malloc не удался\n");
    exit(1);
  }
  printf("Адрес буфера в куче: 0x%lx, размер: 10000 байт\n", (uint64)heap_buf);
  pagetable_print();

  printf("\nТест 4: Снимаем флаги A и D\n");
  page_flags_clear(heap_buf, 10000, (1L << 6) | (1L << 7));
  printf("Флаги A и D сняты\n");
  pagetable_print();

  int check_result = page_flags_check(heap_buf, 10000, (1L << 6) | (1L << 7));
  printf("Проверка флагов A и D (должно быть 0): %d\n", check_result);

  printf("\nТест 5: Читаем данные из буфера\n");
  int sum = 0;
  for (int i = 0; i < 2500; i++)
    sum += heap_buf[i];
  printf("Прочитали данные\n");
  pagetable_print();

  check_result = page_flags_check(heap_buf, 10000, (1L << 6));
  printf("Проверка флага A после чтения (должно быть 1): %d\n", check_result);

  printf("\nТест 6: Пишем данные в буфер\n");
  for (int i = 0; i < 2500; i++)
    heap_buf[i] = i;
  printf("Записали данные\n");
  pagetable_print();

  check_result = page_flags_check(heap_buf, 10000, (1L << 7));
  printf("Проверка флага D после записи (должно быть 1): %d\n", check_result);

  printf("\nТест 7: Освобождаем память\n");
  free(heap_buf);
  printf("Память освобождена\n");
  pagetable_print();

  printf("\nТест 8: Массив на стеке\n");
  int stack_array[512];
  printf("Адрес массива на стеке: 0x%lx\n", (uint64)stack_array);
  for (int i = 0; i < 512; i++)
    stack_array[i] = i;
  printf("Заполнили массив на стеке\n");
  pagetable_print();

  printf("\nВсе тесты завершены\n");
  exit(0);
}
