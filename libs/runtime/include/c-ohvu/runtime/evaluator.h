typedef enum ovru_result {
	OVRU_SUCCESS,
	OVRU_INVALID_CALL_TARGET,
	OVRU_ARGUMENT_COUNT_MISMATCH
} ovru_result;

ovru_result ovru_eval(ovs_instruction i);

