/*
 * Copyright (c) 2006 Jakub Jermar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <test.h>
#include <atomic.h>

const char *test_atomic1(void)
{
	atomic_int a;

	atomic_store(&a, 10);
	if (atomic_load(&a) != 10)
		return "Failed atomic_store()/atomic_load()";

	if (atomic_postinc(&a) != 10)
		return "Failed atomic_postinc()";
	if (atomic_load(&a) != 11)
		return "Failed atomic_load() after atomic_postinc()";

	if (atomic_postdec(&a) != 11)
		return "Failed atomic_postdec()";
	if (atomic_load(&a) != 10)
		return "Failed atomic_load() after atomic_postdec()";

	if (atomic_preinc(&a) != 11)
		return "Failed atomic_preinc()";
	if (atomic_load(&a) != 11)
		return "Failed atomic_load() after atomic_preinc()";

	if (atomic_predec(&a) != 10)
		return "Failed atomic_predec()";
	if (atomic_load(&a) != 10)
		return "Failed atomic_load() after atomic_predec()";

	return NULL;
}
