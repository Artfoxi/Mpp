Задание 15: Синхронизация вывода 5
Условие: Если вы решили задачи 12 и 14, объясните, почему ваше доказательство неприменимо к семафор-счетчикам.
Ответ: В задаче 12 мы доказали, что два мьютекса без других средств синхронизации не могут обеспечить строгое чередование, потому что мьютексы не предоставляют механизма ожидания очереди: они только защищают критические секции, но не управляют порядком выполнения потоков.
Это доказательство неприменимо к семафор-счетчикам по следующим причинам:
1.	Семафоры имеют встроенный механизм ожидания и управления очередностью: 
-	Семафоры (sem_t) поддерживают счетчик, который позволяет "разрешать" или "запрещать" выполнение потока. Когда счетчик равен 0, поток, вызывающий sem_wait, блокируется, пока другой поток не вызовет sem_post и не увеличит счетчик.
-	В задаче 14 мы использовали два семафора: sem_parent и sem_child. Один поток ждет, пока другой не завершит свою итерацию и не вызовет sem_post, что обеспечивает строгое чередование.
2.	Мьютексы не могут "сигнализировать": 
-	Мьютексы предназначены только для защиты ресурсов. Если поток освобождает мьютекс, нет гарантии, что другой поток сразу захватит его — планировщик может снова дать управление первому потоку.
-	Семафоры, напротив, позволяют одному потоку "сигнализировать" другому, увеличивая счетчик и разблокируя ожидающий поток.
3.	Семафоры решают проблему чередования: 
-	В задаче 14 мы инициализировали sem_parent значением 1 (родительский поток начинает), а sem_child — значением 0 (дочерний поток ждет). Это гарантирует, что потоки будут чередоваться, так как каждый поток ждет своей очереди, пока другой не завершит свою работу.
Вывод: Доказательство из задачи 12 неприменимо к семафорам, потому что семафоры предоставляют дополнительный механизм синхронизации (счетчик и ожидание), которого нет у мьютексов. Семафоры позволяют управлять очередностью выполнения потоков, что делает их подходящими для задачи строгого чередования.
