#include "Helpers.h"

#include <cstdarg>
#include <cstdio>

namespace yuxx {
namespace Debug {
	// @brief コンソール画面にフォーマット付きの文字列を表示
	// @param printf 形式の format
	// @param 可変長引数
	// @remarks この関数はデバッグ用です。デバッグ時にしか動作しません。
	void DebugOutputFormatString(const char* format, ...)
	{
#ifdef _DEBUG
		va_list valist;
		va_start(valist, format);
		vprintf(format, valist);
		va_end(valist);
#endif // _DEBUG
	}
}
}
