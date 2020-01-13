#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#define BUF_SIZE            1024

#define OP_LD   0
#define OP_ST   1
#define OP_ADD  2
#define OP_SUB  3
#define OP_MULT 4
#define OP_DIV  5
#define OP_HALT 6


#define MEM_LENGTH_SIM 4096
#define MAX_LINE_LENGTH 500
#define HALT_INST 0x06000000



static char yes_no[2][4] = { "No", "Yes" };
//static char yes_no[2][4] = {"Yes", "No"};
static char units_names[6][4] = { "LD", "ST", "ADD", "SUB", "MUL", "DIV" };
static char units_names_low[6][4] = { "ld", "st", "add", "sub", "mul", "div" };
/*
	Instruction structure
	Any value is document by the comment above it
*/
typedef struct {
	// Instruction
	int inst;

	// Clock cycle that issue of this instruction occured.
	int issue;

	// Clock cycle that read of this instruction occured.
	int read;

	// Clock cycle that execution of this instruction finished.
	int exec;

	// Clock cycle that write back of this instruction occured.
	int write;

	// Instruction opcode
	int opcode;

	// Instruction destination
	int dst;

	// Instruction source 0
	int src0;

	// Instruction source 1
	int src1;

	// Instruction immidiate
	int imm;

	// Instruction handkng unit index
	int unit_index;

} Inst;

/*
	Function unit structure.
	Any value is documented by te comment above
*/
typedef struct {
	// 0 for non-busy, 1 for busy
	int busy;

	// register index for dest
	int f_i;

	// register index for src0
	int f_j;

	// register index for src1
	int f_k;

	// which type function unit works on src0.
	int q_j_type;

	// which index of function unit works on src0.
	int q_j_idx;

	// which type function unit works on src0.
	int q_k_type;

	// which index of function unit works on src0.
	int q_k_idx;

	// 0 for no, 1 for yes.
	int r_j;

	// 0 for no, 1 for yes.
	int r_k;

	// type of the function unit
	int type;

	// the number of clock cycle for this unit to finish
	int delay;

	// the number of remaining clock cycle for this unit to finish.
	int remain;

	// unit index number.
	int index;

	// unit result value.
	float result;

	// the instruction this unit in handle
	Inst *inst_ptr;

	// the instruction index that this unit in handle
	int inst_idx;

	// Flag indicates this unit invloves in WAW
	int waw_flag;
} Unit;


/*
	array - Array of units structures
	used - number of unit in the array
	size - number of allocated units to the array
*/
typedef struct
{
	Unit *array;
	size_t used;
	size_t size;
} Unit_arr;


/*
	The following functions get the insturction value as int and parse data from it
	Each function parse diferent value by its name
*/
unsigned int parserOpcode(unsigned int inst) {
	unsigned int opcode;
	opcode = inst >> 24;
	return opcode;
}
unsigned int parserDst(unsigned int inst) {
	unsigned int dst;
	inst <<= 8;
	dst = inst >> 28;
	return dst;
}
unsigned int parserSrc0(unsigned int inst) {
	unsigned int src0;
	inst <<= 12;
	src0 = inst >> 28;
	return src0;
}
unsigned int parserSrc1(unsigned int inst) {
	unsigned int src1;
	inst <<= 16;
	src1 = inst >> 28;
	return src1;
}
unsigned int parserImm(unsigned int inst) {
	unsigned int imm;
	imm = inst & 0xFFF;
	return imm;
}

//Gets a unit element and initialize its values intial values.
void init_unit(Unit *element)
{
	element->busy = 0;
	element->delay = -1;
	element->f_i = -1;
	element->f_j = -1;
	element->f_k = -1;
	element->index = -1;
	element->q_j_idx = -1;
	element->q_j_type = -1;
	element->q_k_idx = -1;
	element->q_k_type = -1;
	element->remain = -1;
	element->r_j = 1;
	element->r_k = 1;
	element->type = -1;
	element->result = -1;
	element->inst_idx = -1;
	element->inst_ptr = NULL;
	element->waw_flag = 0;
}

//Gets a unit element and resets its value. (f's, r's, q's, remain, instruction...)
void reset_unit(Unit *element)
{
	element->busy = 0;
	element->f_i = -1;
	element->f_j = -1;
	element->f_k = -1;
	element->q_j_idx = -1;
	element->q_j_type = -1;
	element->q_k_idx = -1;
	element->q_k_type = -1;
	element->remain = -1;
	element->r_j = 1;
	element->r_k = 1;
	element->result = -1;
	element->inst_idx = -1;
	element->inst_ptr = NULL;
	element->waw_flag = 0;

}

// Creates any empty instruction object
Inst init_inst()
{
	Inst inst;
	inst.inst = 0;
	inst.exec = -1;
	inst.issue = -1;
	inst.read = -1;
	inst.write = -1;
	inst.opcode = -1;
	inst.dst = -1;
	inst.src0 = -1;
	inst.src1 = -1;
	inst.imm = -1;
	inst.unit_index = -1;

	return inst;
}

// Creates a full instruction element by the parsed data from the input value of the instruction.
Inst createInst(int inst) {
	Inst i = init_inst();
	i.inst = inst;
	i.opcode = parserOpcode(inst);

	i.dst = parserDst(inst);
	i.src0 = parserSrc0(inst);
	i.src1 = parserSrc1(inst);
	i.imm = parserImm(inst);

	return i;
}

// Initializing an empty Unit_arr if size initialSize
void init_unit_array(Unit_arr *a, size_t initialSize)
{
	// Allocate initial space
	a->array = (Unit*)malloc(initialSize * sizeof(Unit));

	a->used = 0;
	a->size = initialSize;
	for (unsigned int i = 0; i < initialSize; i++)
	{
		memset(&a->array[i], 0, sizeof(Unit));
	}
}

// Free the memory of the Unit_arr
void free_unit_array(Unit_arr *a)
{
	// Now free the array 
	free(a->array);
	a->array = NULL;
	a->used = 0;
	a->size = 0;
}


// Inserts new Unit into the Unit_arr, If the array is full its realloactes twice its memory.
void insert_unit_array(Unit_arr *a, Unit element)
{
	if (a->used == a->size)
	{
		a->size *= 2;
		a->array = (Unit *)realloc(a->array, a->size * sizeof(Unit));
	}
	// Copy element fields.
	a->array[a->used].busy = element.busy;
	a->array[a->used].delay = element.delay;
	a->array[a->used].f_i = element.f_i;
	a->array[a->used].f_j = element.f_j;
	a->array[a->used].f_k = element.f_k;
	a->array[a->used].index = element.index;
	a->array[a->used].q_j_idx = element.q_j_idx;
	a->array[a->used].q_j_type = element.q_j_type;
	a->array[a->used].q_k_idx = element.q_k_idx;
	a->array[a->used].q_k_type = element.q_k_type;
	a->array[a->used].inst_ptr = element.inst_ptr;
	a->array[a->used].inst_idx = element.inst_idx;
	a->array[a->used].remain = element.remain;
	a->array[a->used].r_j = element.r_j;
	a->array[a->used].r_k = element.r_k;
	a->array[a->used].type = element.type;
	a->array[a->used].result = element.result;

	a->used++;
}


// Get a value of single precision as 32 bit long and converts it to float
float single_pre_to_float(unsigned long sp) {
	unsigned long sign, exp, fra_bits, fra_b;
	int i = 0;
	float res, fra = 1.0;
	if (sp == 0) {
		return 0.0;
	}
	sign = sp >> 31;
	exp = sp << 1 >> 24;
	fra_bits = sp << 9 >> 9;
	for (i = 0; i < 23; i++) {
		fra_b = fra_bits << (i + 9);
		fra_b = fra_b >> 31;
		fra += fra_b * powf(2, -1 * (i + 1));
	}
	res = powf(-1, sign) * powf(2, exp - 127) * fra;

	return res;
}

// Get a value of float and converts it to single precision as 32 bit int
int floatToSinglePre(float f) {
	int sign, exp, fra, exp_bit_len, i = 0, fra_to_bit = 0;
	int integer = (int)floor(f);
	int res;
	float rat = f - integer;
	sign = f > 0.0 ? 0 : 1;
	exp_bit_len = floor(log2(integer));
	exp = exp_bit_len + 127;
	fra = integer << (23 - exp_bit_len);
	for (i = 0; i < (23 - exp_bit_len); i++) {
		rat = rat * 2;
		if (1 > rat) {
			fra_to_bit <<= 1;
		}
		else {
			fra_to_bit += 1;
			fra_to_bit <<= 1;
			rat -= 1;
		}
	}
	fra_to_bit >>= 1;
	fra = fra | fra_to_bit;
	fra = fra & 0x7FFFFF;
	res = (((sign << 8) + exp) << 23) + fra;

	return res;
}
/*
	Get the configuration text file path and opens it, parsing from it the units details for the input type.
	At the end the fu array is fulled with units according to the configuration file.
*/
int init_units(char* cfg_path, Unit_arr * fu, int type) {
	int units = -1, delay = -1, i;
	char units_str[20];
	char delay_str[20];
	char *ret;
	FILE* config;
	char config_buf[BUF_SIZE];
	char unit_name[7];
	Unit u;
	Unit *u_ptr = &u;

	// parsing the text to search inside the file
	strcpy(units_str, units_names_low[type]);
	strcat(units_str, "_nr_units");
	strcpy(delay_str, units_names_low[type]);
	strcat(delay_str, "_delay");

	config = fopen(cfg_path, "r");
	if (config == NULL) {
		printf("couldn't open the config file");
		return 0;
	}

	while (fgets(config_buf, BUF_SIZE, config) != NULL) {
		ret = strstr(config_buf, units_str);
		if (NULL != ret) {
			if (47 < ret[strlen(ret) - 3] && 58 > ret[strlen(ret) - 3]) {
				units = (10 * (ret[strlen(ret) - 3] - '0')) + (ret[strlen(ret) - 2] - '0');

			}
			else {
				units = ret[strlen(ret) - 2] - '0';
			}
		}
		else {
			ret = strstr(config_buf, delay_str);
			if (NULL != ret) {
				if (47 < ret[strlen(ret) - 3] && 58 > ret[strlen(ret) - 3]) {
					delay = (10 * (ret[strlen(ret) - 3] - '0')) + (ret[strlen(ret) - 2] - '0');

				}
				else {
					delay = ret[strlen(ret) - 2] - '0';
				}
			}
		}
	}

	fclose(config);
	if (-1 == units || -1 == delay) {
		printf("error at init_unit of type: %s", units_names[type]);
		return 0;
	}

	for (i = 0; i < units; i++) {
		init_unit(u_ptr);
		u.type = type;
		u.delay = delay;
		u.index = i;
		u.inst_ptr = -1;
		insert_unit_array(fu, u);
	}

	return 1;
}

/*
	Reads from the configuration text file the desired unit to trace.
	return a 2 int array such that the first item is the type of the unit and the second item is the index of that unit.
*/
int getTraceUnit(char* cfg_path) {
	int unit_index = -1, i, unit_name_len;
	int trace[2];
	char trace_str[20];
	char *ret;
	FILE* config;
	char config_buf[BUF_SIZE];

	strcpy(trace_str, "trace_unit = ");

	config = fopen(cfg_path, "r");
	if (config == NULL) {
		printf("couldn't open the config file");
		return 0;
	}

	while (fgets(config_buf, BUF_SIZE, config) != NULL) {
		ret = strstr(config_buf, trace_str);
		if (NULL != ret) {

			switch (ret[strlen(trace_str)]) {
			case 'A':
				unit_name_len = 3;
				trace[0] = OP_ADD;
				break;
			case 'L':
				unit_name_len = 2;
				trace[0] = OP_LD;
				break;
			case 'M':
				unit_name_len = 3;
				trace[0] = OP_MULT;
				break;
			case 'D':
				unit_name_len = 3;
				trace[0] = OP_DIV;
				break;
			case 'S':
				if (ret[strlen(trace_str)] == 'U') {
					// SU -> SUB
					unit_name_len = 3;
					trace[0] = OP_SUB;
				}
				else {
					unit_name_len = 2;
					trace[0] = OP_ST;
				}
				break;
			}

			if (47 > ret[strlen(trace_str) + unit_name_len + 1] || 58 < ret[strlen(trace_str) + unit_name_len + 1]) {
				unit_index = ret[strlen(trace_str) + unit_name_len] - '0';
			}
			else {
				unit_index = (10 * (ret[strlen(trace_str) + unit_name_len] - '0')) + (ret[strlen(trace_str) + unit_name_len + 1] - '0');
			}
			trace[1] = unit_index;
		}
	}

	fclose(config);

	return trace;
}

void setUnitInstIdx(Unit_arr * fu, int dst, int src) {
	int i = 0, units_size = (int)fu->size;
	for (i = 0; i < units_size; i++) {
		if (fu->array[i].inst_idx == src) {
			fu->array[i].inst_idx = dst;
		}
	}
}

/*
	Moves an instruction inside the Queue from src to dst
*/
void moveInstInQueue(Inst *q, int dst, int src, Unit_arr * add, Unit_arr * sub, Unit_arr * mult, Unit_arr * div, Unit_arr * load, Unit_arr * store) {
	q[dst] = q[src];
	q[src] = init_inst();

	setUnitInstIdx(add, dst, src);
	setUnitInstIdx(sub, dst, src);
	setUnitInstIdx(mult, dst, src);
	setUnitInstIdx(div, dst, src);
	setUnitInstIdx(load, dst, src);
	setUnitInstIdx(store, dst, src);
}

/*
	If there are spaces in the queue it narrows them by moving elemnts from right to left.
	Return the most left free spot if free, if no place is free return -1
*/
int organizeQueue(Inst *q, Unit_arr * add, Unit_arr * sub, Unit_arr * mult, Unit_arr * div, Unit_arr * load, Unit_arr * store) {
	int is_free = -1;
	int i = 0, j = 0;
	for (i = 0; i < 16; i++) {
		if (q[i].inst != 0 && q[i].write > 0) {
			q[i] = init_inst();
		}
	}
	for (i = 0; i < 16; i++) {
		if (q[i].inst == 0) { //If this spot in queue is empty			
			for (j = i + 1; j < 16; j++) {
				if (q[j].inst != 0) {
					moveInstInQueue(q, i, j, add, sub, mult, div, load, store);
					break;
				}
			}
		}
		if (j == 16) {
			is_free = i;
			break;
		}
	}
	return is_free;
}

/*
	Gets and instruction value as int and the queue, "fetching" the instucrion to the queue if there is an free space.
*/
int fetch(Inst *q, int inst, Unit_arr * add, Unit_arr * sub, Unit_arr * mult, Unit_arr * div, Unit_arr * load, Unit_arr * store) {
	Inst i;
	int free_spot;
	free_spot = organizeQueue(q, add, sub, mult, div, load, store);
	if (-1 != free_spot) {
		i = createInst(inst);
		q[free_spot] = i;
		return 1;
	}

	
	return 0;
}

/*
	Gets an instruction and proper Unit array and issues that instruction to the array.
	Setting all needed values for both the unit and the instruction elements
*/
int issueFuncUnitArr(int *busy_type, int *busy_idx, Unit_arr * fu, Inst *inst, int inst_idx) {
	int i = 0, units_size = (int)fu->size;
	for (i = 0; i < units_size; i++) {
		if (fu->array[i].busy == 0) {
			inst->unit_index = i;
			fu->array[i].busy = 1;
			fu->array[i].f_i = inst->dst;
			fu->array[i].f_j = inst->src0;
			fu->array[i].f_k = inst->src1;
			if (-1 == busy_type[inst->src0]) {
				fu->array[i].r_j = 1; // src0 is not busy
			}
			else {
				fu->array[i].r_j = 0;
			}
			if (-1 == busy_type[inst->src1]) {
				fu->array[i].r_k = 1; // src1 is not busy
			}
			else {
				fu->array[i].r_k = 0;
			}
			fu->array[i].q_j_type = busy_type[inst->src0];
			fu->array[i].q_j_idx = busy_idx[inst->src0];

			fu->array[i].q_k_type = busy_type[inst->src1];
			fu->array[i].q_k_idx = busy_idx[inst->src1];

			if (busy_type[inst->dst] != -1) { // Some units is writing to the same dest
				fu->array[i].waw_flag = 1;
			}
			else {
				busy_type[inst->dst] = fu->array[i].type;
				busy_idx[inst->dst] = fu->array[i].index;
			}
			
			fu->array[i].inst_ptr = inst;
			fu->array[i].inst_idx = inst_idx;
			return 1;
		}
	}
	return 0;
}

/*
	Going over the instructions queue and issues the upcoming instruction.
*/
void issue(float *F, int *busy_type, int *busy_idx, Inst *q, int cc, Unit_arr * add, Unit_arr * sub, Unit_arr * mult, Unit_arr * div, Unit_arr * load, Unit_arr * store) {
	int i = 0, is_issued = 0, index_to_issue = -1, is_q_empty = 1;
	//for (i = 15; i >= 0; i--) {
	//	if (i == 0 && (q[i].issue == -1) && (q[15].issue != -1)) {
	//		index_to_issue = 0;
	//		break;
	//	}
	//	if ((q[i].issue == -1) && (q[i - 1].issue != -1)) { // i is at a non-issued instruction and its previos has been issued
	//		index_to_issue = i;
	//		break;
	//	}
	//}
	for (i = 0; i < 16; i++) {
		if (q[i].issue == -1) { //If this spot in queue is empty

			switch (q[i].opcode) {
			case OP_ADD:
				is_issued = issueFuncUnitArr(busy_type, busy_idx, add, &q[i], i);
				break;
			case OP_SUB:
				is_issued = issueFuncUnitArr(busy_type, busy_idx, sub, &q[i], i);
				break;
			case OP_MULT:
				is_issued = issueFuncUnitArr(busy_type, busy_idx, mult, &q[i], i);
				break;
			case OP_DIV:
				is_issued = issueFuncUnitArr(busy_type, busy_idx, div, &q[i], i);
				break;
			case OP_LD:
				is_issued = issueFuncUnitArr(busy_type, busy_idx, load, &q[i], i);
				break;
			case OP_ST:
				is_issued = issueFuncUnitArr(busy_type, busy_idx, store, &q[i], i);
				break;
			}
			if (is_issued) {
				q[i].issue = cc;
			}
			break;
		}
	}
}

/*
	The following functions handle each step of the scoreboard algorithm, each one executes every cycle.
	Each function goes over all of the functional units by going over each type array of units.
	For every units it check if the handle can be exectued.
*/
void readOper(float *F, int *busy_type, int *busy_idx, Inst *q, int cc, Unit_arr * add, Unit_arr * sub, Unit_arr * mult, Unit_arr * div, Unit_arr * load, Unit_arr * store) {
	int i = 0;
	// Going over Add units
	for (i = 0; i < add->used; i++) {
		if ((add->array[i].busy == 1) &&  (add->array[i].inst_idx != -1) && add->array[i].r_j == 1 && add->array[i].r_k == 1 && cc > q[add->array[i].inst_idx].issue && (-1 != q[add->array[i].inst_idx].issue)) {
			if (add->array[i].inst_idx != -1 && q[add->array[i].inst_idx].read == -1) {
				if (add->array[i].waw_flag) {
					if (busy_idx[add->array[i].f_i] == -1) { // This unit dest register is free (WAW)
						busy_type[add->array[i].f_i] = add->array[i].type;
						busy_idx[add->array[i].f_i] = add->array[i].index;
					}
				}

				if (busy_idx[add->array[i].f_i] == -1 || (busy_idx[add->array[i].f_i] == add->array[i].index && busy_type[add->array[i].f_i] == OP_ADD)) { // This unit dest register is free (WAW)
					q[add->array[i].inst_idx].read = cc;
					add->array[i].remain = add->array[i].delay - 1;
					add->array[i].q_j_idx = -1;
					add->array[i].q_k_idx = -1;
				}
			}
		}
	}
	// Going over Sub units
	for (i = 0; i < sub->used; i++) {
		if (sub->array[i].inst_idx != -1 && sub->array[i].r_j == 1 && sub->array[i].r_k == 1 && cc >  q[sub->array[i].inst_idx].issue && -1 !=  q[sub->array[i].inst_idx].issue) {
			if (sub->array[i].inst_idx != -1 &&  q[sub->array[i].inst_idx].read == -1) {
				if (sub->array[i].waw_flag) {
					if (busy_idx[sub->array[i].f_i] == -1) { // This unit dest register is free (WAW)
						busy_type[sub->array[i].f_i] = sub->array[i].type;
						busy_idx[sub->array[i].f_i] = sub->array[i].index;
					}
				}

				if (busy_idx[sub->array[i].f_i] == -1 || (busy_idx[sub->array[i].f_i] == sub->array[i].index && busy_type[sub->array[i].f_i] == OP_SUB)) { // This unit dest register is free (WAW)
					 q[sub->array[i].inst_idx].read = cc;
					sub->array[i].remain = sub->array[i].delay - 1;
				}
			}
		}
	}
	// Going over Mult units
	for (i = 0; i < mult->used; i++) {
		if (mult->array[i].inst_idx != -1 && mult->array[i].r_j == 1 && mult->array[i].r_k == 1 && cc > q[mult->array[i].inst_idx].issue && -1 != q[mult->array[i].inst_idx].issue) {
			if (mult->array[i].inst_idx != -1 && q[mult->array[i].inst_idx].read == -1) {
				if (mult->array[i].waw_flag) {
					if (busy_idx[mult->array[i].f_i] == -1) { // This unit dest register is free (WAW)
						busy_type[mult->array[i].f_i] = mult->array[i].type;
						busy_idx[mult->array[i].f_i] = mult->array[i].index;
					}
				}
				if (busy_idx[mult->array[i].f_i] == -1 || (busy_idx[mult->array[i].f_i] == mult->array[i].index && busy_type[mult->array[i].f_i] == OP_MULT)) { // This unit dest register is free (WAW)
					q[mult->array[i].inst_idx].read = cc;
					mult->array[i].remain = mult->array[i].delay - 1;
				}
			}
		}
	}
	// Going over Div units
	for (i = 0; i < div->used; i++) {
		if (div->array[i].inst_idx != -1 && div->array[i].r_j == 1 && div->array[i].r_k == 1 && cc > q[div->array[i].inst_idx].issue && -1 != q[div->array[i].inst_idx].issue) {
			if (q[div->array[i].inst_idx].read == -1) {
				if (div->array[i].waw_flag) {
					if (busy_idx[div->array[i].f_i] == -1) { // This unit dest register is free (WAW)
						busy_type[div->array[i].f_i] = div->array[i].type;
						busy_idx[div->array[i].f_i] = div->array[i].index;
					}
				}
				if (busy_idx[div->array[i].f_i] == -1 || (busy_idx[div->array[i].f_i] == div->array[i].index && busy_type[div->array[i].f_i] == OP_DIV)) { // This unit dest register is free (WAW)
					q[div->array[i].inst_idx].read = cc;
					div->array[i].remain = div->array[i].delay - 1;
				}
			}
		}
	}
	// Going over Load units
	for (i = 0; i < load->used; i++) {
		if (load->array[i].inst_idx != -1 && cc > q[load->array[i].inst_idx].issue && -1 != q[load->array[i].inst_idx].issue) {
			if (q[load->array[i].inst_idx].read == -1) {
				if (load->array[i].waw_flag) {
					if (busy_idx[load->array[i].f_i] == -1) { // This unit dest register is free (WAW)
						busy_type[load->array[i].f_i] = load->array[i].type;
						busy_idx[load->array[i].f_i] = load->array[i].index;
					}
				}
				if (busy_idx[load->array[i].f_i] == -1 || (busy_idx[load->array[i].f_i] == load->array[i].index && busy_type[load->array[i].f_i] == OP_LD)) { // This unit dest register is free (WAW)
					q[load->array[i].inst_idx].read = cc;
					load->array[i].remain = load->array[i].delay - 1;

				}
			}
		}
	}
	// Going over Store units
	for (i = 0; i < store->used; i++) {
		if (store->array[i].inst_idx != -1 && store->array[i].r_k == 1 && cc > q[store->array[i].inst_idx].issue && -1 != q[store->array[i].inst_idx].issue) {
			if (q[store->array[i].inst_idx].read == -1) {
				q[store->array[i].inst_idx].read = cc;
				store->array[i].remain = store->array[i].delay - 1;
			}
		}
	}
}

void execComp(float *F, int *busy_type, int *busy_idx, Inst *q, int cc, Unit_arr * add, Unit_arr * sub, Unit_arr * mult, Unit_arr * div, Unit_arr * load, Unit_arr * store, int *MEM) {
	int i = 0, load_temp, j = 0;
	// Goinf over Add units
	for (i = 0; i < add->used; i++) {
		if (add->array[i].r_j == 1 && add->array[i].r_k == 1) {
			if (add->array[i].remain >= 0 && q[add->array[i].inst_idx].read <= cc && q[add->array[i].inst_idx].read > 0) { // last cycle this fu completed read operation.
				busy_type[add->array[i].f_i] = OP_ADD;
				busy_idx[add->array[i].f_i] = add->array[i].index;

				if (add->array[i].result == -1) {
					add->array[i].result = F[add->array[i].f_j] + F[add->array[i].f_k];
				}
				add->array[i].remain--;
				if (add->array[i].remain <= 0) {
					q[add->array[i].inst_idx].exec = cc;
				}
			}
		}
	}
	// Going over Sub units
	for (i = 0; i < sub->used; i++) {
		if (sub->array[i].r_j == 1 && sub->array[i].r_k == 1) {
			if (sub->array[i].remain > 0 && q[sub->array[i].inst_idx].read < cc) { // last cycle this fu completed read operation.
				if (sub->array[i].result == -1) {
					sub->array[i].result = F[sub->array[i].f_j] - F[sub->array[i].f_k];
				}
				sub->array[i].remain--;
				if (sub->array[i].remain == 0) {
					 q[sub->array[i].inst_idx].exec = cc;
				}
			}
		}
	}
	// Going over MUlt units
	for (i = 0; i < mult->used; i++) {
		if (mult->array[i].r_j == 1 && mult->array[i].r_k == 1) {
			if (mult->array[i].remain > 0 && q[mult->array[i].inst_idx].read < cc) { // last cycle this fu completed read operation.
				if (mult->array[i].result == -1) {
					mult->array[i].result = F[mult->array[i].f_j] * F[mult->array[i].f_k];
				}
				mult->array[i].remain--;
				if (mult->array[i].remain == 0) {
					q[mult->array[i].inst_idx].exec = cc;
				}
			}
		}
	}
	// Going over Div units
	for (i = 0; i < div->used; i++) {
		if (div->array[i].r_j == 1 && div->array[i].r_k == 1) {
			if (div->array[i].remain > 0 && q[div->array[i].inst_idx].read < cc) { // last cycle this fu completed read operation.
				if (div->array[i].result == -1) {
					div->array[i].result = F[div->array[i].f_j] / F[div->array[i].f_k];
				}
				div->array[i].remain--;
				if (div->array[i].remain == 0) {
					q[div->array[i].inst_idx].exec = cc;
				}
			}
		}
	}
	// Going over Load units
	for (i = 0; i < load->used; i++) {
		if (load->array[i].remain > 0 && q[load->array[i].inst_idx].read < cc) { // last cycle this fu completed read operation.
			busy_type[load->array[i].f_i] = OP_LD;
			busy_idx[load->array[i].f_i] = load->array[i].index;
			if (load->array[i].result == -1) {
				load_temp = MEM[q[load->array[i].inst_idx].imm];
				load->array[i].result = single_pre_to_float(load_temp);
			}
			load->array[i].remain--;

			if (load->array[i].remain == 0) {
				q[load->array[i].inst_idx].exec = cc;
			}
		}
	}
	// Going over Store units
	for (i = 0; i < store->used; i++) {
		if (store->array[i].r_k == 1) {
			if (store->array[i].remain > 0 && q[store->array[i].inst_idx].read < cc) { // last cycle this fu completed read operation.
				busy_type[store->array[i].f_i] = OP_ST;
				busy_idx[store->array[i].f_i] = store->array[i].index;
				if (store->array[i].result == -1) {
					store->array[i].result = F[store->array[i].f_k];
				}
				store->array[i].remain--;

				// To check if load inst colide with this store inst, if so, delay the store execution
				for (j = 0; j < load->used; j++) {
					// Check if addresses values of store and load collide
					if (load->array[j].inst_idx != -1) {
						if (q[load->array[j].inst_idx].imm == q[store->array[i].inst_idx].imm) {
							// Check that colided load inst is issued before store
							if (q[load->array[j].inst_idx].issue < q[store->array[i].inst_idx].issue) {
								// Check if load instruction finished it execution
								if (q[load->array[j].inst_idx].exec <= cc) {
									// If load inst has not finished its execution delayed store 1 more cycle
									if (store->array[i].remain == 0) {
										store->array[i].remain++;
									}
								}
							}
						}
					}
				}
				if (store->array[i].remain == 0) {
					MEM[q[store->array[i].inst_idx].imm] = floatToSinglePre(store->array[i].result);
					q[store->array[i].inst_idx].exec = cc;
				}
			}
		}
	}
}

void writeBack(float *F, int *busy_type, int *busy_idx, Inst *q, int cc, Unit_arr * add, Unit_arr * sub, Unit_arr * mult, Unit_arr * div, Unit_arr * load, Unit_arr * store) {
	int i = 0;
	// Going over Add units
	for (i = 0; i < add->used; i++) {

		if (add->array[i].inst_idx != -1 && q[add->array[i].inst_idx].exec > 0 && q[add->array[i].inst_idx].exec < cc) { // last cycle this fu completed read operation
			if (add->array[i].remain <= 0) {
				F[add->array[i].f_i] = add->array[i].result;
				q[add->array[i].inst_idx].write = cc;
				if (busy_type[q[add->array[i].inst_idx].dst] == OP_ADD && busy_idx[q[add->array[i].inst_idx].dst] == i) {
					busy_type[q[add->array[i].inst_idx].dst] = -1;
					busy_idx[q[add->array[i].inst_idx].dst] = -1;
				}

				reset_unit(&add->array[i]);
			}
		}
	}

	// Going over Sub units
	for (i = 0; i < sub->used; i++) {
		if (sub->array[i].remain == 0) {
			if ( q[sub->array[i].inst_idx].exec < cc) { // last cycle this fu completed read operation.
				F[sub->array[i].f_i] = sub->array[i].result;
				 q[sub->array[i].inst_idx].write = cc;
				 if (busy_type[q[sub->array[i].inst_idx].dst] == OP_SUB && busy_idx[q[sub->array[i].inst_idx].dst] == i) {
					 busy_type[q[sub->array[i].inst_idx].dst] = -1;
					 busy_idx[q[sub->array[i].inst_idx].dst] = -1;
				 }
				reset_unit(&sub->array[i]);
			}
		}
	}

	// Going over Mult units
	for (i = 0; i < mult->used; i++) {
		if (mult->array[i].remain == 0) {
			if (q[mult->array[i].inst_idx].exec < cc) { // last cycle this fu completed read operation.
				F[mult->array[i].f_i] = mult->array[i].result;
				q[mult->array[i].inst_idx].write = cc;
				if (busy_type[q[mult->array[i].inst_idx].dst] == OP_MULT && busy_idx[q[mult->array[i].inst_idx].dst] == i) {
					busy_type[q[mult->array[i].inst_idx].dst] = -1;
					busy_idx[q[mult->array[i].inst_idx].dst] = -1;
				}
				reset_unit(&mult->array[i]);
			}
		}
	}

	// Going over Div units
	for (i = 0; i < div->used; i++) {
		if (div->array[i].remain == 0) {
			if (q[div->array[i].inst_idx].exec < cc) { // last cycle this fu completed read operation.
				F[div->array[i].f_i] = div->array[i].result;
				q[div->array[i].inst_idx].write = cc;
				if (busy_type[q[div->array[i].inst_idx].dst] == OP_DIV && busy_idx[q[div->array[i].inst_idx].dst] == i) {
					busy_type[q[div->array[i].inst_idx].dst] = -1;
					busy_idx[q[div->array[i].inst_idx].dst] = -1;
				}
				reset_unit(&div->array[i]);

			}
		}
	}

	// Going over Load units
	for (i = 0; i < load->used; i++) {
		if (load->array[i].remain == 0) {
			if (q[load->array[i].inst_idx].exec < cc) { // last cycle this fu completed read operation.
				F[load->array[i].f_i] = load->array[i].result;
				q[load->array[i].inst_idx].write = cc;
				if (busy_type[q[load->array[i].inst_idx].dst] == OP_LD && busy_idx[q[load->array[i].inst_idx].dst] == i) {
					busy_type[load->array[i].f_i] = -1;
					busy_idx[load->array[i].f_i] = -1;
				}
				reset_unit(&load->array[i]);
			}
		}
	}
	// Going over Store units
	for (i = 0; i < store->used; i++) {
		if (store->array[i].remain == 0) {
			if (q[store->array[i].inst_idx].exec < cc) { // last cycle this fu completed read operation.
				q[store->array[i].inst_idx].write = cc;
				if (busy_type[q[store->array[i].inst_idx].dst] == OP_ST && busy_idx[q[store->array[i].inst_idx].dst] == i) {
					busy_type[q[store->array[i].inst_idx].dst] = -1;
					busy_idx[q[store->array[i].inst_idx].dst] = -1;
				}
				reset_unit(&store->array[i]);
			}
		}
	}
}

/*
	Extra function to clear the Status array and notify the units for next cycle which of the registers is free to read from.
*/

void clearBusyReg(float *F, int *busy_type, int *busy_idx, Inst *q, int cc, Unit_arr * add, Unit_arr * sub, Unit_arr * mult, Unit_arr * div, Unit_arr * load, Unit_arr * store) {
	int i = 0;
	int src0, src1;
	// Going over Add units
	for (i = 0; i < add->used; i++) {
		if (busy_idx[add->array[i].f_j] == -1) {
			add->array[i].r_j = 1;
			add->array[i].q_j_idx = -1;
		}
		if (busy_idx[add->array[i].f_k] == -1) {
			add->array[i].r_k = 1;
			add->array[i].q_k_idx = -1;
		}
	}

	// Going over Sub units
	for (i = 0; i < sub->used; i++) {
		if (busy_idx[sub->array[i].f_j] == -1) {
			sub->array[i].r_j = 1;
			sub->array[i].q_j_idx = -1;
		}
		if (busy_idx[sub->array[i].f_k] == -1) {
			sub->array[i].r_k = 1;
			sub->array[i].q_k_idx = -1;
		}
	}

	// Going over Mult units
	for (i = 0; i < mult->used; i++) {
		if (busy_idx[mult->array[i].f_j] == -1) {
			mult->array[i].r_j = 1;
			mult->array[i].q_j_idx = -1;
		}
		if (busy_idx[mult->array[i].f_k] == -1) {
			mult->array[i].r_k = 1;
			mult->array[i].q_k_idx = -1;
		}
	}

	// Going over div units
	for (i = 0; i < div->used; i++) {
		if (busy_idx[div->array[i].f_j] == -1) {
			div->array[i].r_j = 1;
			div->array[i].q_j_idx = -1;
		}
		if (busy_idx[div->array[i].f_k] == -1) {
			div->array[i].r_k = 1;
			div->array[i].q_k_idx = -1;
		}
	}

	// Going over load units
	for (i = 0; i < load->used; i++) {
		if (busy_idx[load->array[i].f_j] == -1) {
			load->array[i].r_j = 1;
			load->array[i].q_j_idx = -1;
		}
		if (busy_idx[load->array[i].f_k] == -1) {
			load->array[i].r_k = 1;
			load->array[i].q_k_idx = -1;
		}
	}

	// Going over store units
	for (i = 0; i < store->used; i++) {
		if (busy_idx[store->array[i].f_j] == -1) {
			store->array[i].r_j = 1;
			store->array[i].q_j_idx = -1;
		}
		if (busy_idx[store->array[i].f_k] == -1) {
			store->array[i].r_k = 1;
			store->array[i].q_k_idx = -1;
		}
	}

}


int main(int argc, char** argv) {
	// Declarations
	Unit_arr fu_sub, fu_mult, fu_add, fu_divide, fu_load, fu_store;
	float F[16];
	// The status array is designed by 2 array of int, for each register index the arrays are indicates which unit has handle to it
	// 1 array is to indicate the unit type, second array is for the unit index
	int busy_type[16];
	int busy_idx[16];
	Inst q[16];
	// For memory in
	int MEM[MEM_LENGTH_SIM] = { 0 };
	int num_line = 0, inst_num = 0, i;
	int cc = 1;
	// For the trace unit, same as status array, both values indicates the type and index of the unit.
	int t_type, t_index;
	int *trace_unit_name;
	char *line;
	// flag if halt wa reached
	int halt_reached = 0;

	// For printing purposes, for the trace unit its r_j and r_k values.
	int to_print_r_j = 0, to_print_r_k = 0;
	// Flag to indicates if the simulation is running
	int sim = 1;

	// "remebers" which instruction is needed to be written next by the issue order.
	int issue_to_print = 1;

	// String variables for printing purposes.
	char q_j[6];
	char q_k[6];

	// File pointers
	FILE* memin;
	FILE* memout;
	FILE* trace_inst;
	FILE* trace_unit;
	FILE* regout;

	//Allocations
	line = calloc(1, sizeof(char)*MAX_LINE_LENGTH);
	if (line == NULL) {
		printf("Fail to calloc line\n");
		return 0;
	}

	//Initialization
	init_unit_array(&fu_add, 1);
	init_units(argv[1], &fu_add, OP_ADD);

	init_unit_array(&fu_sub, 1);
	init_units(argv[1], &fu_sub, OP_SUB);

	init_unit_array(&fu_mult, 1);
	init_units(argv[1], &fu_mult, OP_MULT);

	init_unit_array(&fu_divide, 1);
	init_units(argv[1], &fu_divide, OP_DIV);

	init_unit_array(&fu_load, 1);
	init_units(argv[1], &fu_load, OP_LD);

	init_unit_array(&fu_store, 1);
	init_units(argv[1], &fu_store, OP_ST);


	// Inits instructions queue
	for (i = 0; i < 16; i++) {
		q[i] = init_inst();
	}

	// Inits registers
	for (i = 0; i < 16; i++) {
		F[i] = 1.0 * i;
		busy_idx[i] = -1;
		busy_type[i] = -1;
	}

	//Scaning input memory to MEM
	memin = fopen(argv[2], "r");
	if (memin == NULL) {
		printf("couldn't open the memin file");
		return 0;
	}
	trace_inst = fopen(argv[5], "w");
	if (trace_inst == NULL) {
		printf("couldn't open the traceinst file");
		return 0;
	}
	trace_unit = fopen(argv[6], "w");
	if (trace_unit == NULL) {
		printf("couldn't open the trace_unit file");
		return 0;
	}


	while (fgets(line, MAX_LINE_LENGTH, memin) != NULL) {
		sscanf(line, "%x", &MEM[num_line]);
		num_line++;
	}

	i = 0;
	// Doing the first fetch before starts to run.
	inst_num += fetch(q, MEM[inst_num], &fu_add, &fu_sub, &fu_mult, &fu_divide, &fu_load, &fu_store);
	issue(F, busy_type, busy_idx, q, cc, &fu_add, &fu_sub, &fu_mult, &fu_divide, &fu_load, &fu_store);
	cc++;
	trace_unit_name = getTraceUnit(argv[1]);
	t_type = trace_unit_name[0];
	t_index = trace_unit_name[1];

	while (sim) {

		strcpy(q_j, "-");
		strcpy(q_k, "-");
		switch (t_type) {
		case OP_ADD:
			if (fu_add.array[t_index].busy == 1) {
				fprintf(trace_unit, "%d %s%d", cc, units_names[OP_ADD], fu_add.array[t_index].index);
				fprintf(trace_unit, " F%d F%d F%d", fu_add.array[t_index].f_i, fu_add.array[t_index].f_j, fu_add.array[t_index].f_k);
				to_print_r_j = fu_add.array[t_index].r_j;
				to_print_r_k = fu_add.array[t_index].r_k;
				if (fu_add.array[t_index].inst_ptr->exec > 0) {
					to_print_r_j = 0;;
					to_print_r_k = 0;;
				}
				if (fu_add.array[t_index].q_j_idx != -1) {
					sprintf(q_j, "%s%d", units_names[fu_add.array[t_index].q_j_type], fu_add.array[t_index].q_j_idx);
				}
				if (fu_add.array[t_index].q_k_idx != -1) {
					sprintf(q_k, "%s%d", units_names[fu_add.array[t_index].q_k_type], fu_add.array[t_index].q_k_idx);
				}
				fprintf(trace_unit, " %s %s %s %s\n", q_j, q_k, yes_no[to_print_r_j], yes_no[to_print_r_k]);
			}
			break;
		case OP_SUB:
			if (fu_sub.array[t_index].busy == 1) {
				fprintf(trace_unit, "%d %s%d", cc, units_names[OP_SUB], fu_sub.array[t_index].index);
				fprintf(trace_unit, " F%d F%d F%d", fu_sub.array[t_index].f_i, fu_sub.array[t_index].f_j, fu_sub.array[t_index].f_k);
				to_print_r_j = fu_sub.array[t_index].r_j;
				to_print_r_k = fu_sub.array[t_index].r_k;
				if (fu_sub.array[t_index].inst_ptr->exec > 0) {
					to_print_r_j = 0;;
					to_print_r_k = 0;;
				}
				if (fu_sub.array[t_index].q_j_idx != -1) {
					sprintf(q_j, "%s%d", units_names[fu_sub.array[t_index].q_j_type], fu_sub.array[t_index].q_j_idx);
				}
				if (fu_sub.array[t_index].q_k_idx != -1) {
					sprintf(q_k, "%s%d", units_names[fu_sub.array[t_index].q_k_type], fu_sub.array[t_index].q_k_idx);
				}
				fprintf(trace_unit, " %s %s %s %s\n", q_j, q_k, yes_no[to_print_r_j], yes_no[to_print_r_k]);
			}
			break;
		case OP_MULT:
			if (fu_mult.array[t_index].busy == 1) {
				fprintf(trace_unit, "%d %s%d", cc, units_names[OP_MULT], fu_mult.array[t_index].index);
				fprintf(trace_unit, " F%d F%d F%d", fu_mult.array[t_index].f_i, fu_mult.array[t_index].f_j, fu_mult.array[t_index].f_k);
				to_print_r_j = fu_mult.array[t_index].r_j;
				to_print_r_k = fu_mult.array[t_index].r_k;
				if (fu_mult.array[t_index].inst_ptr->exec > 0) {
					to_print_r_j = 0;;
					to_print_r_k = 0;;
				}
				if (fu_mult.array[t_index].q_j_idx != -1) {
					sprintf(q_j, "%s%d", units_names[fu_mult.array[t_index].q_j_type], fu_mult.array[t_index].q_j_idx);
				}
				if (fu_mult.array[t_index].q_k_idx != -1) {
					sprintf(q_k, "%s%d", units_names[fu_mult.array[t_index].q_k_type], fu_mult.array[t_index].q_k_idx);
				}
				fprintf(trace_unit, " %s %s %s %s\n", q_j, q_k, yes_no[to_print_r_j], yes_no[to_print_r_k]);
			}
			break;
		case OP_DIV:
			if (fu_divide.array[t_index].busy == 1) {
				fprintf(trace_unit, "%d %s%d", cc, units_names[OP_DIV], fu_divide.array[t_index].index);
				fprintf(trace_unit, " F%d F%d F%d", fu_divide.array[t_index].f_i, fu_divide.array[t_index].f_j, fu_divide.array[t_index].f_k);
				to_print_r_j = fu_divide.array[t_index].r_j;
				to_print_r_k = fu_divide.array[t_index].r_k;
				if (fu_divide.array[t_index].inst_ptr->exec > 0) {
					to_print_r_j = 0;;
					to_print_r_k = 0;;
				}
				if (fu_divide.array[t_index].q_j_idx != -1) {
					sprintf(q_j, "%s%d", units_names[fu_divide.array[t_index].q_j_type], fu_divide.array[t_index].q_j_idx);
				}
				if (fu_divide.array[t_index].q_k_idx != -1) {
					sprintf(q_k, "%s%d", units_names[fu_divide.array[t_index].q_k_type], fu_divide.array[t_index].q_k_idx);
				}
				fprintf(trace_unit, " %s %s %s %s\n", q_j, q_k, yes_no[to_print_r_j], yes_no[to_print_r_k]);
			}
			break;
		case OP_LD:
			if (fu_load.array[t_index].busy == 1) {
				fprintf(trace_unit, "%d %s%d", cc, units_names[OP_LD], fu_load.array[t_index].index);
				fprintf(trace_unit, " F%d F%d F%d", fu_load.array[t_index].f_i, fu_load.array[t_index].f_j, fu_load.array[t_index].f_k);
				to_print_r_j = fu_load.array[t_index].r_j;
				to_print_r_k = fu_load.array[t_index].r_k;
				if (fu_load.array[t_index].inst_ptr->exec > 0) {
					to_print_r_j = 0;;
					to_print_r_k = 0;;
				}
				if (fu_load.array[t_index].q_j_idx != -1) {
					sprintf(q_j, "%s%d", units_names[fu_load.array[t_index].q_j_type], fu_load.array[t_index].q_j_idx);
				}
				if (fu_load.array[t_index].q_k_idx != -1) {
					sprintf(q_k, "%s%d", units_names[fu_load.array[t_index].q_k_type], fu_load.array[t_index].q_k_idx);
				}
				fprintf(trace_unit, " %s %s %s %s\n", q_j, q_k, yes_no[to_print_r_j], yes_no[to_print_r_k]);
			}
			break;
		case OP_ST:
			if (fu_store.array[t_index].busy == 1) {
				fprintf(trace_unit, "%d %s%d", cc, units_names[OP_ST], fu_store.array[t_index].index);
				fprintf(trace_unit, " F%d F%d F%d", fu_store.array[t_index].f_i, fu_store.array[t_index].f_j, fu_store.array[t_index].f_k);
				to_print_r_j = fu_store.array[t_index].r_j;
				to_print_r_k = fu_store.array[t_index].r_k;
				if (fu_store.array[t_index].inst_ptr->exec > 0) {
					to_print_r_j = 0;;
					to_print_r_k = 0;;
				}
				if (fu_store.array[t_index].q_j_idx != -1) {
					sprintf(q_j, "%s%d", units_names[fu_store.array[t_index].q_j_type], fu_store.array[t_index].q_j_idx);
				}
				if (fu_store.array[t_index].q_k_idx != -1) {
					sprintf(q_k, "%s%d", units_names[fu_store.array[t_index].q_k_type], fu_store.array[t_index].q_k_idx);
				}
				fprintf(trace_unit, " %s %s %s %s\n", q_j, q_k, yes_no[to_print_r_j], yes_no[to_print_r_k]);
			}
			break;
		}



		if (MEM[inst_num] != HALT_INST) {
			inst_num += fetch(q, MEM[inst_num], &fu_add, &fu_sub, &fu_mult, &fu_divide, &fu_load, &fu_store);
		}
		else {
			halt_reached = 1;
		}
		issue(F, busy_type, busy_idx, q, cc, &fu_add, &fu_sub, &fu_mult, &fu_divide, &fu_load, &fu_store);
		readOper(F, busy_type, busy_idx, q, cc, &fu_add, &fu_sub, &fu_mult, &fu_divide, &fu_load, &fu_store);
		execComp(F, busy_type, busy_idx, q, cc, &fu_add, &fu_sub, &fu_mult, &fu_divide, &fu_load, &fu_store, MEM);
		writeBack(F, busy_type, busy_idx, q, cc, &fu_add, &fu_sub, &fu_mult, &fu_divide, &fu_load, &fu_store);
		clearBusyReg(F, busy_type, busy_idx, q, cc, &fu_add, &fu_sub, &fu_mult, &fu_divide, &fu_load, &fu_store);

		cc++;


		if (halt_reached) {
			sim = 0;
			for (i = 0; i < 16; i++) {
				if (q[i].issue >= issue_to_print) {
					sim = 1;
					break;
				}
			}

		}
		for (i = 0; i < 16; i++) {
			if (q[i].issue == issue_to_print) {
				if (q[i].write > 0) {
					fprintf(trace_inst, "%.8X %d %s%d %d %d %d %d\n", q[i].inst, issue_to_print - 1, units_names[q[i].opcode], q[i].unit_index, q[i].issue, q[i].read, q[i].exec, q[i].write);
					issue_to_print++;
				}
				break;
			}
			//If gets here none of the instruction is issued to printing
			if (i == 15) {
				issue_to_print++;
			}
		}
	}
	fclose(trace_inst);


	regout = fopen(argv[4], "w");
	if (regout == NULL) {
		printf("couldn't open the regout file");
		return 0;
	}
	for (i = 0; i < 16; i++) {
		fprintf(regout, "%f\n", F[i]);
	}

	memout = fopen(argv[3], "w");
	if (memout == NULL) {
		printf("couldn't open the memout file");
		return 0;
	}
	for (i = 0; i < MEM_LENGTH_SIM; i++) {
		fprintf(memout, "%.8X\n", MEM[i]);
	}

	fclose(memout);

	return 0;
}