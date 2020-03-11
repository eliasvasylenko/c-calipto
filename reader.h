typedef struct reader_api {
	int64_t (*cursor_position)(int32_t inputDepth);

	int32_t (*cursor_depth)(void);

	void* (*read)(void);

	void* (*read_symbol)(void);

	bool (*read_step_in)(void);

	int32_t (*read_step_out)(void);
} reader_api;
