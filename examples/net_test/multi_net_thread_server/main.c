#include <stdio.h>

#include "gateway.h"

/*  ʹ��server-model01Ŀ¼�е���·����� : ���ÿ�������̵߳ȴ�IOCP    */
/*  ��Ϣ��ֱ���������¼��д���  */

int main()
{
    gateway_init(4002);
    gateway_start();
    getchar();
}