// myTafLoader.cpp : Defines the entry point for the console application.
//


#include "stdafx.h"
#include "getopt.h"
#include <conio.h>

// прототип функции
typedef int(__stdcall* tMyFunction)(const int16_t *, size_t, TAF_header& hdr);

bool get_samples(std::ifstream &file, TAF_header& hdr, int16_t **src, size_t *len, double startTime, double lenTime)
{
	/// ¬ходные параметры:
	// file - ссылка на объект файла
	// startTime [мс] - врем€ начала нужного участка
	// lenTime [мс] - длина нужного участка

	/// ¬ыходные параметры
	// hdr - заголовок
	// srs - по этому указателю "живет" выборка
	// len - длина в действительных отсчетах

	(*len) = 0;

	// „тение заголовка
	file.read((char *)&hdr, sizeof(TAF_header));;

	// Ќомер отсчета есть произведение частоты дискретизации и времени
	uint64_t start_sample = (uint64_t)(startTime * hdr.SF) * 1000;
	uint64_t size_sample = (uint64_t)(lenTime * hdr.SF) * 1000;

	if (size_sample > hdr.ChunkNumber * hdr.ChunkSize - start_sample) {
		printf("”казанный размер выходных данных превышает размер файла\n");
	}

	try {
		(*src) = new int16_t[2 * size_sample];
	}
	catch (...)
	{
		printf("не удалось выделить требуемый участок пам€ти\n");
		return false;
	}

	// номер блока, с которого считываем данные
	uint64_t num_block_start = (uint64_t)floor((start_sample - 1) / hdr.ChunkSize) + 1;
	// номер блока, по который считываем данные
	uint64_t num_block_stop = (uint64_t)floor((start_sample + size_sample - 1) / hdr.ChunkSize) + 1;


	if (hdr.DataType == 0) // complex<short>
	{
		// размеры файлов оставл€ют желать меньшего
		uint64_t seekg_arg = (uint64_t)(num_block_start - 1)*(uint64_t)(hdr.ChunkSize * 4 + 8);
		file.seekg(seekg_arg, std::ios::cur); // смещаемс€ относительно текущей позиции

		double time_offset;

		// „тение временной метки 
		file.read((char*)&time_offset, sizeof(double));

		// —мещаемс€ внутри chunk
		int p = start_sample - hdr.ChunkSize * (num_block_start - 1);
		// на тот случай, если выборка находитс€ в одном чанке
		size_t size_buf = 2 * min(hdr.ChunkSize - p + 1, size_sample);


		file.seekg((p - 1) * 4, std::ios::cur); // смещаемс€ относительно текущей позиции
		file.read((char*)(*src), size_buf * sizeof(int16_t));
		size_t countLength = size_buf;


		for (uint64_t i = num_block_start + 1; i < num_block_stop; i++)
		{

			// „тение временной метки 
			file.read((char*)&time_offset, sizeof(double));
			file.read((char*)&(*src)[countLength], 2 * hdr.ChunkSize * sizeof(int16_t));
			countLength += 2 * hdr.ChunkSize;
		}

		// хвост
		if (countLength != 2 * size_sample) {
			// „тение временной метки 
			file.read((char*)&time_offset, sizeof(double));
			file.read((char*)&(*src)[countLength], (2 * size_sample - countLength) * sizeof(int16_t));

		}
	}
	else
	{
		delete[](*src);
		(*src) = nullptr;
		return false;
	}


	(*len) = 2 * size_sample; // количество действительных отсчетов
	return true;
}

static void usage(void) {
	printf("„то-то пошло не так при разборе аргументов\n\n");
	printf("\t-f <filename>\n");
	printf("\t-s <ms> - start time\n");
	printf("\t-l <ms> - interval time\n");
	printf("\t-m <ZigBee|Bluetooth> - standart\n");
}

enum standart {
	ZigBee = 0,
	Bluetooth = 1
};

#ifdef UNICODE
	LPCWSTR library_name[2] = { L"ZigBee.dll", L"BluetoothDLL.dll" };
#else
	LPCSTR library_name[2] = { "ZigBee.dll", "BluetoothDLL.dll" };
#endif
LPCSTR function_name[2] = { "FillZigBeeData", "FillBluetoothData" };

int main(int argc, char* argv[])
{
	int opt;
	char* end;
	int standart;

	std::ifstream file;
	double start_time = -1.,
		   interval_time = -1.;

	TAF_header hdr;
	int16_t *src = nullptr;
	size_t size;
	HMODULE hModule = NULL;


	// разбор параметров командной строки
	while ((opt = getopt(argc, argv, "f:s:l:m:")) != EOF) {
		switch (opt) {
		case 'f':
			
			file.open(optarg, std::ios::binary);
			if (!file.is_open())
			{
				printf("Could not open file %s\n", optarg);
				return 1;
			}else {
				printf("file: %s\n", optarg);
			}
			break;
		case 's':
		
			start_time = strtod(optarg, &end);
			printf("start time: %f\n", start_time);
			break;
		case 'l':

			interval_time = strtod(optarg, &end);
			printf("interval: %f\n", interval_time);
			break;
		case 'm':

			if (strcmp("ZigBee", optarg) == 0)
			{
				standart = ZigBee;
				//wchar_t library_name[] = L"ZigBee.dll";
				//wchar_t function_name[] = L"FillZigBeeData";
			}
			else if (strcmp("Bluetooth", optarg) == 0)
			{
				standart = Bluetooth;
				//wchar_t library_name[] = L"BluetoothDLL.dll";
				//wchar_t function_name[] = L"FillBluetoothData";
			}
			else
			{
				printf("standart is false: %s\n", optarg);
				return 2;
			}
			break;
		default:
			usage();
			return 3;
		}
	}

	if (start_time < 0 || interval_time <= 0) {
		// TODO: сделать по умолчанию считывание всего файла
		printf("the range is not specified\n");
		usage();
		return 4;
	}


	if (!get_samples(file, hdr, &src, &size, start_time, interval_time))
	{
		printf("Could not get samples from file\n");
		return 13;
	}

	// закрываем файл, он больше нам не нужен
	file.close();

	printf("\n\tReceiverFreq: %f\n", hdr.ReceiverFreq);
	printf("\tBand: %f\n", hdr.Band);
	printf("\tSF: %f\n", hdr.SF);
	printf("\tDataType: %d\n", hdr.DataType);

	// —оздание dump-а

	//std::fstream tab("file.data", std::ios::binary | std::ios::out);
	//{
	//	// пишем в файл длину массива:
	//	tab.write((char *)&size, sizeof(size));
	//	// теперь сам массив: 
	//	tab.write((char*)src, sizeof(int16_t) * size);
	//	tab.close();
	//}

	//// „тение
	//std::fstream show("file.data", std::ios::binary | std::ios::in);
	//
	//size_t size_in_file;
	//// читаем длину массива
	//show.read((char *)&size_in_file, sizeof(size_in_file));
	//// читаем саму массив
	//int16_t *temp = new int16_t[size_in_file];
	//show.read((char*)temp, sizeof(int16_t) * size_in_file);
	//if (show)
	//	printf("all array read successfully.");

	// «агрузка dll 

	hModule = LoadLibrary(library_name[standart]);
	if (NULL != hModule)
	{
		tMyFunction FillData = (tMyFunction)GetProcAddress(hModule, function_name[standart]);

		// ¬ызов функции обработки ZigBee
		int back = FillData((const int16_t*)src, size, hdr);

		// ¬ыгрузка dll
		FreeLibrary(hModule);
	}
	else {
		wprintf(L"%s don't loaded with code: %d\n", library_name[standart], GetLastError());
	}

	if (nullptr != src)
		delete[] src;

	_getch();
	return 0;
}

