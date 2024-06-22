#pragma once

#include <Windows.h>

#pragma pack(push, 1)

typedef struct {
	union {
		UINT16 ushort0;
		struct {
			UINT16 SN : 4;
			UINT16 CID : 12;
		};
	};
	BYTE CID_header_type;
}compressed_ip_packet_typedef;

typedef struct {
	union {
		UINT32 ulong0;
		struct {
			UINT32 flow_label : 20;
			UINT32 trafiic_class : 8;
			UINT32 version : 4;
		};
	};
	BYTE next_header;
	BYTE hop_limit;
	BYTE source_address[16];
	BYTE destination_address[16];
}IPv6_header_wo_length_typedef;

typedef struct {
	UINT16 source_port;
	UINT16 destination_port;
}UDP_header_wo_length_typedef;

typedef struct {
	union {
		BYTE byte0;
		struct {
			BYTE RAP_flag : 1;
			BYTE extension_flag : 1;
			BYTE reserved0 : 1;
			BYTE FEC_type : 2;
			BYTE packet_counter_flag : 1;
			BYTE version : 2;
		};
	};
	union {
		BYTE byte1;
		struct {
			BYTE payload_type : 6;
			BYTE reserved1 : 2;
		};
	};
	UINT16 packet_id;
	UINT32 timestamp;
	UINT32 packet_sequence_number;
}MMTP_packet_typedef;

typedef struct {
	UINT16 extension_type;
	UINT16 extension_length;
}extension_typedef;

typedef struct {
	UINT16 extension_type;
	UINT16 extension_length;
	union {
		BYTE byte0;
		struct {
			BYTE MMT_scramble_init_control : 1;
			BYTE message_auth_control : 1;
			BYTE scramble_ident_control : 1;
			BYTE MMT_scramble_control : 2;
			BYTE reserved0 : 3;
		};
	};
}header_extension_typedef;


typedef struct {
	UINT16 payload_length;
	union {
		BYTE byte0;
		struct {
			BYTE aggregation_flag : 1;
			BYTE fragmentation_indicator : 2;
			BYTE timed_flag : 1;
			BYTE fragment_type : 4;
		};
	};
	BYTE fragment_counter;
	UINT32 MPU_sequence_number;
}MMTP_payload_typedef;

typedef struct {
	UINT32 movie_fragment_sequence_number;
	UINT32 sample_number;
	UINT32 offset;
	BYTE priority;
	BYTE dependency_counter;
}MFU_payload_typedef;

typedef struct {
	UINT16 data_unit_length;
	UINT32 movie_fragment_sequence_number;
	UINT32 sample_number;
	UINT32 offset;
	BYTE priority;
	BYTE dependency_counter;
}MFU_AGGREGATE_payload_typedef;

typedef struct {
	UINT32 item_id;
}MFU_Nontimed_payload_typedef;

typedef struct {
	UINT16 data_unit_length;
	UINT32 item_id;
}MFU_Nontimed_AGGREGATE_payload_typedef;


typedef struct {
	union {
		BYTE byte0;
		struct {
			BYTE aggregation_flag : 1;
			BYTE length_extension_flag : 1;
			BYTE reserved : 4;
			BYTE fragmentation_indicator : 2;
		};
	};
	BYTE fragment_counter;
}SIGNALLING_message_typedef;

typedef struct {
	BYTE version;
	UINT32 length;
	BYTE number_of_tables;
}PA_message_typedef;

typedef struct {
	BYTE table_id;
	BYTE version;
	UINT16 length;
}PATable_typedef;

typedef struct {
	BYTE version;
	UINT16 length;
	BYTE table_id;
}M2_message_typedef;

typedef struct {
	BYTE version;
	UINT16 length;
}CA_message_typedef;

typedef struct {
	BYTE table_id;
	BYTE version;
	UINT16 length;
}CATable_typedef;


typedef struct {
	BYTE table_id;
	BYTE version;
	UINT16 length;
	union {
		BYTE byte0;
		struct {
			BYTE MPT_mode : 2;
			BYTE reserved : 6;
		};
	};
	BYTE MMT_package_id_length;
}MPTable_typedef;

typedef struct {
	BYTE table_id;
	BYTE version;
	UINT16 length;
	BYTE num_of_package;
	BYTE MMT_package_id_length;
}PLTable_typedef;

typedef struct {
	BYTE identifier_type;
	UINT32 asset_id_scheme;
	BYTE asset_id_length;
}Asset_typedef;

typedef struct {
	BYTE asset_type[4];
	union {
		BYTE byte0;
		struct {
			BYTE asset_clock_relation_flag : 1;
			BYTE reserved : 7;
		};
	};
	BYTE location_count;
	BYTE location_type;
}Asset2_typedef;


//typedef struct {
//	BYTE version;
//	UINT16 length;
//	BYTE table_id;
//	union {
//		UINT16 ushort0;
//		struct {
//			UINT16 section_length : 12;
//			UINT16 sync1 : 2;
//			UINT16 sync0 : 1;
//			UINT16 section_syntax_indicator : 1;
//		};
//	};
//	UINT16 table_id_extension;
//	union {
//		BYTE byte0;
//		struct {
//			BYTE current_next_indicator : 1;
//			BYTE version_number : 5;
//			BYTE sync2 : 2;
//		};
//	};
//	BYTE section_number;
//	BYTE last_section_number;
//}M2_message_typedef;


typedef struct {
	union {
		UINT16 ushort0;
		struct {
			UINT16 section_length : 12;
			UINT16 reserved0 : 2;
			UINT16 reserved_future_use : 1;
			UINT16 section_syntax_indicator : 1;
		};
	};
	UINT16 service_id;
	union {
		BYTE byte0;
		struct {
			BYTE current_next_indicator : 1;
			BYTE version_number : 5;
			BYTE reserved1 : 2;
		};
	};
	BYTE section_number;
	BYTE last_section_number;
	UINT16 tlv_stream_id;
	UINT16 original_network_id;
	BYTE segment_last_section_number;
	BYTE last_table_id;
	UINT16 event_id;
	union {
		ULONG64 ulong0;
		struct {
			ULONG64 duration : 24;
			ULONG64 start_time : 40;
		};
	};
	union {
		UINT16 ushort1;
		struct {
			UINT16 descriptors_loop_denth : 12;
			UINT16 free_CA_mode : 1;
			UINT16 running_status : 3;
		};
	};
}MHEVT_InfoTable_typedef;

typedef struct {
	union {
		UINT16 ushort0;
		struct {
			UINT16 section_length : 12;
			UINT16 reserved0 : 2;
			UINT16 reserved_future_use : 1;
			UINT16 section_syntax_indicator : 1;
		};
	};
	UINT16 service_id;
	union {
		BYTE byte0;
		struct {
			BYTE current_next_indicator : 1;
			BYTE version_number : 5;
			BYTE reserved1 : 2;
		};
	};
	BYTE section_number;
	BYTE last_section_number;
	UINT16 tlv_stream_id;
	UINT16 original_network_id;
	BYTE segment_last_section_number;
	BYTE last_table_id;
}MHEVT_Schedule_InfoTable_typedef;

typedef struct {
	UINT16 event_id;
	union {
		ULONG64 ulong0;
		struct {
			ULONG64 duration : 24;
			ULONG64 start_time : 40;
		};
	};
	union {
		UINT16 ushort1;
		struct {
			UINT16 descriptors_loop_length : 12;
			UINT16 free_CA_mode : 1;
			UINT16 running_status : 3;
		};
	};
}MHEVT_Schedule2_InfoTable_typedef;



typedef struct {
	UINT16 descriptor_length;
	BYTE ISO_639_language_code[3];
	UINT8 event_name_length;
}MHShort_EventDescriptor_typedef;


typedef struct {
	UINT16 descriptor_length;
	union {
		BYTE byte0;
		struct {
			BYTE last_descriptor_number : 4;
			BYTE descriptor_number : 4;
		};
	};
	BYTE ISO_639_language_code[3];
	UINT16 length_of_items;
	BYTE item_description_length;
}MHExtended_EventDescriptor_typedef;


typedef struct {
	UINT16 descriptor_tag;
	BYTE descriptor_length;
}MMTSI_typedef_struct;


typedef struct {
	BYTE table_id;
	union {
		UINT16 ushort0;
		struct {
			UINT16 section_length : 12;
			UINT16 sync1 : 2;
			UINT16 sync0 : 1;
			UINT16 section_syntax_indicator : 1;
		};
	};
	UINT16 table_id_extension;
	union {
		BYTE byte0;
		struct {
			BYTE current_next_indicator : 1;
			BYTE version_number : 5;
			BYTE sync2 : 2;
		};
	};
	BYTE section_number;
	BYTE last_section_number;
}Signaling_packet_typedef;

typedef struct {
	BYTE version;
	UINT16 length;
	BYTE table_id;
	union {
		UINT16 ushort0;
		struct {
			UINT16 section_length : 12;
			UINT16 sync1 : 2;
			UINT16 sync0 : 1;
			UINT16 section_syntax_indicator : 1;
		};
	};
	BYTE JST_time[5];
	union {
		UINT16 ushort1;
		struct {
			UINT16 descriptor_loop_length : 12;
			UINT16 reserved : 4;
		};
	};
}MHTime_Offset_table_typedef;

#pragma pack(pop)
