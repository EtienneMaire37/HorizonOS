#pragma once

#include <mlibc/all-sysdeps.hpp>
#include <string.h>

static inline __attribute__((noreturn)) void STUB(const char* msg)
{
	const char* str = "\nSTUB: ";
	ssize_t unused;
	mlibc::sys_write(2, str, strlen(str), &unused);
	mlibc::sys_write(2, msg, strlen(msg), &unused);
	mlibc::sys_write(2, "\n", 1, &unused);
	while (true);
	__builtin_unreachable();
}