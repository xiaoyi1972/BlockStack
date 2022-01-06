#include "TetrisBase.h"

namespace std {
	std::string to_string(const Oper& a) {
		std::string str;
		switch (a) {
		case Oper::None: str += "None"; break;
		case Oper::Left: str += "Left"; break;
		case Oper::Right: str += "Right"; break;
		case Oper::SoftDrop: str += "SoftDrop"; break;
		case Oper::HardDrop: str += "HardDrop"; break;
		case Oper::Hold: str += "Hold"; break;
		case Oper::Cw: str += "Cw"; break;
		case Oper::Ccw: str += "Ccw"; break;
		case Oper::DropToBottom: str += "DropToBottom"; break;
		}
		return str;
	}

	std::string to_string(const Piece& p) {
		std::string str;
		switch (p) {
		case Piece::None:str += " "; break;
		case Piece::O:str += "O"; break;
		case Piece::T:str += "T"; break;
		case Piece::I:str += "I"; break;
		case Piece::S:str += "S"; break;
		case Piece::Z:str += "Z"; break;
		case Piece::L:str += "L"; break;
		case Piece::J:str += "J"; break;
		}
		return str;
	}

	std::string to_string(const TSpinType& p) {
		std::string str;
		switch (p) {
		case TSpinType::None:str += "None"; break;
		case TSpinType::TSpinMini:str += "TSpinMini"; break;
		case TSpinType::TSpin:str += "TSpin"; break;
		}
		return str;
	}

	std::string to_string(const std::vector<Oper>& a) {
		std::string str;
		for (const auto v : a)
			str += std::to_string(v) + ' ';
		return str;
	}

	std::ostream& operator<<(std::ostream& out, const Oper& a) {
		out << std::to_string(a);
		return out;
	}

	std::ostream& operator<<(std::ostream& out, const std::vector<Oper>& a) {
		out << std::to_string(a);
		return out;
	}

	std::ostream& operator<<(std::ostream& out, const Piece& p) {
		out << std::to_string(p);
		return out;
	}

	std::ostream& operator<<(std::ostream& out, const TSpinType& t) {
		out << std::to_string(t);
		return out;
	}

}