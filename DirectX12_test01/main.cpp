#include <Windows.h>
#ifdef _DEBUG
#include <iostream>
#endif // _DEBUG

using namespace std;

// @brief �R���\�[����ʂɃt�H�[�}�b�g�t���̕������\��
// @param printf �`���� format
// @param �ϒ�����
// @remarks ���̊֐��̓f�o�b�O�p�ł��B�f�o�b�O���ɂ������삵�܂���B
void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif // _DEBUG
}

#ifdef _DEBUG
int main()
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif // _DEBUG
{
	DebugOutputFormatString("Show window test.");
	getchar();
	
	return 0;
}
