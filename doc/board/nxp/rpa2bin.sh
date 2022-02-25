#!/bin/sh

touch "${2}"

sed "/^\s*\(#.*\)\?$/ d;s@\s*#.*@@" "${1}" | tr -s " \t" "  " | while read op sop addr width val ; do
	case "${op}" in
	memory)
		a=$(printf "%08x\n" "${addr}" | sed "s@\(..\)\(..\)\(..\)\(..\)@\4 \3 \2 \1@")
		v=$(printf "%08x\n" "${val}" | sed "s@\(..\)\(..\)\(..\)\(..\)@\4 \3 \2 \1@")
		case "${sop}" in
		set)
			printf "00 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		setbit)
			printf "01 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		clrbit)
			printf "02 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		chkbit1)
			printf "03 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		*)
			echo "Unknown SOP : ${sop}"
			exit 1
			;;
		esac
		;;

	freq0)
		a=$(printf "%08x\n" "${addr}" | sed "s@\(..\)\(..\)\(..\)\(..\)@\4 \3 \2 \1@")
		v=$(printf "%08x\n" "${val}" | sed "s@\(..\)\(..\)\(..\)\(..\)@\4 \3 \2 \1@")
		case "${sop}" in
		set)
			printf "07 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		setbit)
			printf "08 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		clrbit)
			printf "09 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		chkbit1)
			printf "0a 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		*)
			echo "Unknown SOP : ${sop}"
			exit 1
			;;
		esac
		;;

	freq1)
		a=$(printf "%08x\n" "${addr}" | sed "s@\(..\)\(..\)\(..\)\(..\)@\4 \3 \2 \1@")
		v=$(printf "%08x\n" "${val}" | sed "s@\(..\)\(..\)\(..\)\(..\)@\4 \3 \2 \1@")
		case "${sop}" in
		set)
			printf "0c 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		setbit)
			printf "0d 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		clrbit)
			printf "0e 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		chkbit1)
			printf "0f 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		*)
			echo "Unknown SOP : ${sop}"
			exit 1
			;;
		esac
		;;

	freq2)
		a=$(printf "%08x\n" "${addr}" | sed "s@\(..\)\(..\)\(..\)\(..\)@\4 \3 \2 \1@")
		v=$(printf "%08x\n" "${val}" | sed "s@\(..\)\(..\)\(..\)\(..\)@\4 \3 \2 \1@")
		case "${sop}" in
		set)
			printf "11 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		setbit)
			printf "12 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		clrbit)
			printf "13 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		chkbit1)
			printf "14 00 00 00  %s  %s  %02x 00 00 00\n" "$a" "$v" "$width" | xxd -r -p >> ${2}
			;;
		*)
			echo "Unknown SOP : ${sop}"
			exit 1
			;;
		esac
		;;

	sysparam)
		case "${sop}" in
		set)
			w=$(printf "%08x\n" "${width}" | sed "s@\(..\)\(..\)\(..\)\(..\)@\4 \3 \2 \1@")
			case "${addr}" in
			fw_version)
				printf "1b 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x11 "${w}" | xxd -r -p >> ${2}
				;;
			debug_uart)
				printf "1b 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x10 "${w}" | xxd -r -p >> ${2}
				;;
			*)
				echo "Unknown TYPE : ${addr}"
				exit 1
				;;
			esac
			;;
		*)
			echo "Unknown SOP : ${sop}"
			exit 1
			;;
		esac
		;;

	ddrparam)
		case "${sop}" in
		set)
			w=$(printf "%08x\n" "${width}" | sed "s@\(..\)\(..\)\(..\)\(..\)@\4 \3 \2 \1@")
			case "${addr}" in
			dram_type)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x00 "${w}" | xxd -r -p >> ${2}
				;;
			data_width)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x03 "${w}" | xxd -r -p >> ${2}
				;;
			num_pstat)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x04 "${w}" | xxd -r -p >> ${2}
				;;
			train_2d)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x0d "${w}" | xxd -r -p >> ${2}
				;;
			PhyVref)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x0e "${w}" | xxd -r -p >> ${2}
				;;
			csPresent)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x57 "${w}" | xxd -r -p >> ${2}
				;;
			frequency0)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x05 "${w}" | xxd -r -p >> ${2}
				;;
			pllbypass0)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x09 "${w}" | xxd -r -p >> ${2}
				;;
			frequency1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x06 "${w}" | xxd -r -p >> ${2}
				;;
			pllbypass1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x0a "${w}" | xxd -r -p >> ${2}
				;;
			frequency2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x07 "${w}" | xxd -r -p >> ${2}
				;;
			pllbypass2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x0b "${w}" | xxd -r -p >> ${2}
				;;
			TrainCtrl0)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x84 "${w}" | xxd -r -p >> ${2}
				;;
			TrainCtrl1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x85 "${w}" | xxd -r -p >> ${2}
				;;
			TrainCtrl2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x86 "${w}" | xxd -r -p >> ${2}
				;;
			TrainInfo)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x01 "${w}" | xxd -r -p >> ${2}
				;;
			MR1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x17 "${w}" | xxd -r -p >> ${2}
				;;
			MR2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x1b "${w}" | xxd -r -p >> ${2}
				;;
			MR3)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x1f "${w}" | xxd -r -p >> ${2}
				;;
			MR4)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x23 "${w}" | xxd -r -p >> ${2}
				;;
			MR11)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x2f "${w}" | xxd -r -p >> ${2}
				;;
			MR12)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x33 "${w}" | xxd -r -p >> ${2}
				;;
			MR13)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x37 "${w}" | xxd -r -p >> ${2}
				;;
			MR14)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x3b "${w}" | xxd -r -p >> ${2}
				;;
			MR16)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x3f "${w}" | xxd -r -p >> ${2}
				;;
			MR17)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x43 "${w}" | xxd -r -p >> ${2}
				;;
			MR22)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x47 "${w}" | xxd -r -p >> ${2}
				;;
			MR24)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x4b "${w}" | xxd -r -p >> ${2}
				;;
			MR1-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x18 "${w}" | xxd -r -p >> ${2}
				;;
			MR2-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x1c "${w}" | xxd -r -p >> ${2}
				;;
			MR3-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x20 "${w}" | xxd -r -p >> ${2}
				;;
			MR4-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x24 "${w}" | xxd -r -p >> ${2}
				;;
			MR11-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x30 "${w}" | xxd -r -p >> ${2}
				;;
			MR12-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x34 "${w}" | xxd -r -p >> ${2}
				;;
			MR13-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x38 "${w}" | xxd -r -p >> ${2}
				;;
			MR14-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x3c "${w}" | xxd -r -p >> ${2}
				;;
			MR16-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x40 "${w}" | xxd -r -p >> ${2}
				;;
			MR17-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x44 "${w}" | xxd -r -p >> ${2}
				;;
			MR22-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x48 "${w}" | xxd -r -p >> ${2}
				;;
			MR24-1)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x4c "${w}" | xxd -r -p >> ${2}
				;;
			MR1-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x19 "${w}" | xxd -r -p >> ${2}
				;;
			MR2-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x1d "${w}" | xxd -r -p >> ${2}
				;;
			MR3-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x21 "${w}" | xxd -r -p >> ${2}
				;;
			MR4-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x25 "${w}" | xxd -r -p >> ${2}
				;;
			MR11-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x31 "${w}" | xxd -r -p >> ${2}
				;;
			MR12-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x35 "${w}" | xxd -r -p >> ${2}
				;;
			MR13-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x39 "${w}" | xxd -r -p >> ${2}
				;;
			MR14-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x3d "${w}" | xxd -r -p >> ${2}
				;;
			MR16-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x41 "${w}" | xxd -r -p >> ${2}
				;;
			MR17-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x45 "${w}" | xxd -r -p >> ${2}
				;;
			MR22-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x49 "${w}" | xxd -r -p >> ${2}
				;;
			MR24-2)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x4d "${w}" | xxd -r -p >> ${2}
				;;
			ATxImpedance)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x6e "${w}" | xxd -r -p >> ${2}
				;;
			ODTImpedance)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x66 "${w}" | xxd -r -p >> ${2}
				;;
			TxImpedance)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x6a "${w}" | xxd -r -p >> ${2}
				;;
			lp4x_mode)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x02 "${w}" | xxd -r -p >> ${2}
				;;
			read_dbi)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x0f "${w}" | xxd -r -p >> ${2}
				;;
			extCalRes)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x61 "${w}" | xxd -r -p >> ${2}
				;;
			WDQSExt)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x7f "${w}" | xxd -r -p >> ${2}
				;;
			SlewRiseDQ)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x88 "${w}" | xxd -r -p >> ${2}
				;;
			SlewFallDQ)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x8c "${w}" | xxd -r -p >> ${2}
				;;
			SlewFallAC)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x91 "${w}" | xxd -r -p >> ${2}
				;;
			SlewRiseAC)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x90 "${w}" | xxd -r -p >> ${2}
				;;
			CaliInterval)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x80 "${w}" | xxd -r -p >> ${2}
				;;
			CaliOnce)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x81 "${w}" | xxd -r -p >> ${2}
				;;
			RX2D_trainOpt)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x94 "${w}" | xxd -r -p >> ${2}
				;;
			TX2D_trainOpt)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x95 "${w}" | xxd -r -p >> ${2}
				;;
			Share_2dVref)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x96 "${w}" | xxd -r -p >> ${2}
				;;
			Delay_weight2d)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x97 "${w}" | xxd -r -p >> ${2}
				;;
			Volt_weight2d)
				printf "06 00 00 00  %02x 00 00 00  %s  00 00 00 00\n" 0x98 "${w}" | xxd -r -p >> ${2}
				;;
			*)
				echo "Unknown TYPE : ${addr}"
				exit 1
				;;
			esac
			;;
		*)
			echo "Unknown SOP : ${sop}"
			exit 1
			;;
		esac
		;;


	*)
		echo "Unknown OP : ${op}"
		exit 1
		;;
	esac
done

# Some sort of end marker
printf "a5 a5 a5 a5  00 00 00 00  00 00 00 00  00 00 00 00\n" | xxd -r -p >> ${2}
