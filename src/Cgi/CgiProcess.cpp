#include "CgiProcess.hpp"

CgiProcess::CgiProcess()
	: _pid(), _stdin(), _stdout(), _body(),
	_bodyOffset(0), _exitstatus(0) {}

CgiProcess::~CgiProcess() {}


bool	CgiProcess::start() { return false; }
