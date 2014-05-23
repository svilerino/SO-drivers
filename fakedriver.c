#include "fakeserialhelper.h"
#include <linux/kfifo.h>
#include <linux/semaphore.h>

#define DEVICE_NAME "fakeserial"
#define FAKESERIAL_FIFO_SIZE PAGE_SIZE

void irq_handler_callback(void);
ssize_t read_handler(struct file *, char __user *, size_t, loff_t *);
//cola buffer
struct kfifo _buffer;
//semaforo
struct semaphore mutex_cola;

//es llamada por cuando llegan datos
void irq_handler_callback(){
	//leer con inb
	u8 dataRead = fake_inb(0);
	kfifo_in(&_buffer, &dataRead, sizeof(u8));
}

//es llamado por el consumidor para obtener datos
ssize_t read_handler(struct file * fileParam, char __user * userBuffer, size_t readCount, loff_t * offset){
	down_interruptible(&mutex_cola);
	ssize_t bytesLeidos = 0;
	printk(KERN_DEBUG "fakeserial: Leyendo!\n");

	int colaVacia = kfifo_is_empty(&_buffer);
	while((bytesLeidos<readCount) && (!colaVacia)){
		//leer de la cola
		u8 dataBuf = 0;
		kfifo_out(&_buffer, &dataBuf, sizeof(u8));
		//ahora hay que copiar del buffer de kernel dataBuf al parametro
		//del buffer del usuario USANDO COPY_TO_USER sumandole el offset de la cant. de bytes
		//que habiamos copiado ahi previamente.
		copy_to_user((userBuffer + bytesLeidos), &dataBuf, sizeof(u8));
		bytesLeidos++;

		colaVacia = kfifo_is_empty(&_buffer);
	}
	up(&mutex_cola);
	return bytesLeidos;	
}


struct file_operations file_operations_handler = {
	.read = read_handler,
};

struct miscdevice serial_miscdev = {
	MISC_DYNAMIC_MINOR,
	"fakeserial",
	&file_operations_handler,
};

static int __init fakeserial_init(void) {
	printk(KERN_DEBUG "fakeserial: Inicializando kfifo...");
    kfifo_alloc(&_buffer, PAGE_SIZE, GFP_KERNEL);
    sema_init(&mutex_cola, 1);
    printk(KERN_DEBUG "Ok\n");

    printk(KERN_DEBUG "fakeserial: Registrando device...");
    misc_register(&serial_miscdev);
    printk(KERN_DEBUG "Ok\n");

    printk(KERN_DEBUG "fakeserial: Inicializando device...");
    fake_serial_init(FAKE_SERIAL_MODE_TEXT, irq_handler_callback);
    printk(KERN_DEBUG "Ok\n");

    printk(KERN_DEBUG "fakeserial: Initialized!\n");
    return 0;
}

static void __exit fakeserial_exit(void) {
	printk(KERN_DEBUG "fakeserial: Desregistrando...");
    misc_deregister(&serial_miscdev);
    printk(KERN_DEBUG "Ok\n");

	printk(KERN_DEBUG "fakeserial: Finalizando device...");
    fake_serial_exit();
    printk(KERN_DEBUG "Ok\n");
	printk(KERN_DEBUG "fakeserial: Unloaded!\n");
}

module_init(fakeserial_init);
module_exit(fakeserial_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vilerino, Silvio");
MODULE_DESCRIPTION("Driver del dispositivo serial piola");
