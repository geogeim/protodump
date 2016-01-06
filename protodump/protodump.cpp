// Copyright (c) 2015 by Marian Geoge, Licensed under the MIT license: http://www.opensource.org/licenses/mit-license.php

#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/io/coded_stream.h"


#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#include "buffer.h"

struct logger
{
	struct wnullbuf : std::wstreambuf
	{
		int overflow(int c) { return c; }
	};

	enum level
	{
		INFO,
		WARNING,
		ERROR,
		FATAL
	};

	static const char* level_names[];

	static void set_level(level l_)
	{
		l = l_;
	}

	static std::wostream& info()
	{
		return log(INFO);
	}

	static std::wostream& warning()
	{
		return log(WARNING);
	}

	static std::wostream& error()
	{
		return log(ERROR);
	}

	static std::wostream& fatal()
	{
		return log(FATAL);
	}

	static std::wostream& log(level l_)
	{
		if (l > l_)
		{
			static wnullbuf nb;
			static std::wostream wcnull(&nb);

			return wcnull;
		}

		return std::wcout << level_names[l_] << " ";
	}

private:
	static level l;
};


const char* logger::level_names[] = { "[Info]", "[Warning]", "[Error]", "[Fatal]" };
logger::level logger::l;

struct options
{
	static options& get()
	{
		static options instance;
		return instance;
	}

	options()
	{
		verbose = false;
		descriptor_proto = false;
		unknown_dependencies = false;
		outdir = boost::filesystem::current_path();
	}

	void usage()
	{
		std::wcout << L"protodump. extracts .proto files from binaries\n"
			L"version 1.0.3 (libprotobuf version " << google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION).c_str() << L")\n"
			L"usage: protodump <file file...> [options]\n"
			L"options:\n"
			L"-v\t\t\tverbose\n"
			L"-o <dir>\t\toutput directory, will be created if missing, defaults to current\n"
			L"--descriptor-proto\talso dump descriptor.proto\n"
			L"--unknown-dependencies\tdump definitions even if there are missing dependencies\n"
			L"\t\t\tthey will be replaced with dummy descriptors\n";
	}

	bool read(int argc, wchar_t** argv)
	{
		if(argc < 1)
		{
			usage();
			return false;
		}

		for (int i = 1; i < argc; i++)
		{

			if (std::wstring(argv[i]).compare(L"-v") == 0)
			{
				verbose = true;
				continue;
			}

			if (std::wstring(argv[i]).compare(L"-o") == 0)
			{
				i++; outdir = argv[i];
				continue;
			}

			if (std::wstring(argv[i]).compare(L"--unknown-dependencies") == 0)
			{
				unknown_dependencies = true;
				continue;
			}

			if (std::wstring(argv[i]).compare(L"--descriptor-proto") == 0)
			{
				descriptor_proto = true;
				continue;
			}

			if(boost::filesystem::exists(argv[i]) == false)
			{
				logger::error() << argv[i] << L"' not found\n";
				return false;
			}

			files.push_back(std::wstring(argv[i]));
		}

		if(files.size() == 0)
		{
			usage();
			return false;
		}

		return true;
	}

	bool verbose;
	bool descriptor_proto;
	bool unknown_dependencies;
	boost::filesystem::path outdir;
	std::list<std::wstring> files;
};


struct descriptor
{
	descriptor(const std::string& name_, void* data_, size_t size_)
		: name(name_)
		, data(data_, size_)
	{
	}

	std::string name;
	buffer data;
};




//	searches for a protocol buffer binary tag with number 1, and type string which ends with '.proto'
//	if found invoke the full FileDescriptorProto parser
void search_descriptors(const boost::iostreams::mapped_file_source& map, std::list<descriptor>& descriptors)
{
	for (const char* vptr = map.data(); vptr < map.data() + map.size(); vptr++)
	{
		if (vptr[0] != 0x0A || vptr[1] >= 0x80)
		{
			continue;
		}

		size_t sz = vptr[1];

		if (vptr[sz - 4] != '.' ||
			vptr[sz - 3] != 'p' ||
			vptr[sz - 2] != 'r' ||
			vptr[sz - 1] != 'o' ||
			vptr[sz - 0] != 't' ||
			vptr[sz + 1] != 'o')
		{
			continue;
		}

		google::protobuf::io::CodedInputStream stream((const google::protobuf::uint8 *)vptr, map.size() - (vptr - map.data()));
		google::protobuf::FileDescriptorProto descriptor;

		if (descriptor.MergePartialFromCodedStream(&stream) == false ||
			descriptor.unknown_fields().empty() == false ||
			descriptor.has_name() == false ||
			descriptor.name().empty() == true)
		{
			continue;
		}

		size_t descriptor_size = stream.CurrentPosition() - 1;

		if (options::get().verbose)
		{
			logger::info() << L"found " << descriptor.name().c_str() << " @ 0x" << std::hex << vptr - map.data() << " size " << std::dec << descriptor_size << "\n";
		}

		descriptors.emplace_back(descriptor.name(), (void*)vptr, descriptor_size);

		vptr += descriptor_size;
	}
}

void log_handler(google::protobuf::LogLevel level, const char* filename, int line,
	const std::string& message)
{
	logger::log(static_cast<logger::level>(level)) << message.c_str() << "\n";
}


int wmain(int argc, wchar_t** argv)
{
	if(options::get().read(argc, argv) == false)
	{
		return -1;
	}

	logger::set_level(options::get().verbose ? logger::INFO : logger::WARNING);

	google::protobuf::SetLogHandler(&log_handler);

	std::list<descriptor> descriptors;

	for(auto file : options::get().files)
	{
		if (options::get().verbose)
		{
			logger::info() << L"processing " << file.c_str() << "\n";
		}

		try 
		{
			boost::filesystem::wpath _path(file);
			boost::iostreams::mapped_file_source map(_path);
			search_descriptors(map, descriptors);
			map.close();
		}
		catch(std::exception& e)
		{
			logger::error() << e.what();
			return -1;
		}
	}


	google::protobuf::EncodedDescriptorDatabase db;
	google::protobuf::DescriptorPool dp(&db, nullptr);

	if (options::get().unknown_dependencies == true)
	{
		dp.AllowUnknownDependencies();
	}

	for(auto& descriptor : descriptors)
	{
		google::protobuf::FileDescriptorProto fdp;
		if(db.FindFileByName(descriptor.name, &fdp) == true) // skip already existing files
		{
			continue;
		}

		if(db.Add(descriptor.data.data(), descriptor.data.size()) == false)
		{
			return -1;
		}
	}


	int stat_extracted_files = 0;
	int stat_extracted_messages = 0;

	for (auto& descriptor : descriptors)
	{
		if (descriptor.name.compare("google/protobuf/descriptor.proto") == 0 && options::get().descriptor_proto == false)
		{
			continue;
		}


		const google::protobuf::FileDescriptor* fd = dp.FindFileByName(descriptor.name);
		if(fd == nullptr)
		{
			return -1;
		}


		boost::filesystem::path output_path(options::get().outdir);
		output_path.append(descriptor.name);

		if(output_path.has_parent_path() && boost::filesystem::exists(output_path.parent_path()) == false)
		{
			boost::filesystem::create_directories(output_path.parent_path());
		}

		boost::filesystem::ofstream file(output_path);
		file << fd->DebugString();
		file.close();

		stat_extracted_files++;
		stat_extracted_messages += fd->message_type_count();
	}

	std::wcout << stat_extracted_files << " files, " << stat_extracted_messages << " messages extracted\n";

	return 0;
}
