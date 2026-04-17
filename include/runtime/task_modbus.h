#ifndef RUNTIME_TASK_MODBUS_H
#define RUNTIME_TASK_MODBUS_H

/*  task_modbus — обработка Modbus RTU через USB CDC.
 *  Общая задача для обоих режимов (PRODUCTION и TEST).
 *
 *  Стек:      configMINIMAL_STACK_SIZE
 *  Приоритет: 1 (низкий, ниже UI)
 *  Период:    опрос каждую 1 мс (vTaskDelay)
 */
void task_modbus_start(void);

#endif /* RUNTIME_TASK_MODBUS_H */
