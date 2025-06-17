# Архиватор Хаффмана
## Как начать
Скомпилируйте архиватор используя Makefile
```sh
make
```
Файл *huf* - результат компиляции

## Использование
### Вывод списка команд
```sh
./huf -help
```
### Архивирование
```sh
./huf -compress [files|dirs] -output <file> -word <number> [-aw|-dw]
```
Параметры: \
**-output** выходной архив (по умолчанию "archive.huff") \
**-word** размер кодируемых слов в байтах от 1 до 2 (по умолчанию 1) \
**-dw** пропустить предупреждения о файлах малого размера (<512 байт) \
**-aw** согласиться с предупреждениями о файлах малого размера (<512 байт) \
Примеры: \
Cоздание архива из всех файлов в текущей папке и её подпапок
```sh
./huf -compress .
```
Сжать файл *file.txt* и папку *exampledir*. Сохранить в архив *ar.huff* в папке *out*:
```sh
./huf -compress file.txt exampledir -output out/ar.huff
```
### Деархивирование
```sh
./huf -decompress <archive> -output <dir> [files] [-dir <path>]
```
Параметры: \
**-output** папка, в которую деархивировать файлы (по умолчанию ".") \
**-dir** метка что деархивируется именно папка, а не файл \
Примеры: \
Деархивация в текущую папку
```sh
./huf -decompress archive.huff
```
Деархивация файла *file.txt* и папки *exampledir* в папку *out*
```sh
./huf -decompress archive.huff -output out file.txt -dir exampledir
```
### Просмотр содержимого архива
```sh
./huf -list <archive> -dir <path>
```
Параметры: \
**-dir** папка внутри архива (по умолчанию "") \
Примеры: \
Просмотр содержимого:
```sh
./huf -list archive.huff
```
Просмотр содержимого в папке *exampledir*:
```sh
./huf -list archive.huff -dir exampledir
```
