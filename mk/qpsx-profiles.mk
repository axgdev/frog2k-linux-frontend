# Canonical QPSX profiles for reproducible SF2000 A/B work.
#
# Keep this file declarative: the existing long experiment targets remain
# available, while these four profiles provide a complete, auditable closure
# for the pre-adaptive-DMA frontier.  A profile's build tag and fingerprint
# are passed to the core, so a copied executable cannot lose its identity.

QPSX_PROFILE_ARTIFACT_ROOT ?= build/qpsx-profiles
QPSX_PROFILE_NAMES := \
	frontier-production-control \
	frontier-production-candidate \
	frontier-layout-pad640 \
	frontier-tail-control \
	frontier-tail-candidate

QPSX_PROFILE_PATCH_ID := $(shell sha256sum $(MUFROG_qpsx_PATCHES) | sha256sum | cut -c1-16)

# This is the exact current fastmem frontier recipe.  Every QPSX knob that
# reaches the core is named here, including diagnostic and code-layout knobs;
# the only profile-specific changes below are ASM reads and tail metrics.
QPSX_PROFILE_FRONTIER_COMMON_ARGS := \
	QPSX_PLATFORM=linux \
	QPSX_OPTIMIZE=-O2 QPSX_GPU_OPTIMIZE= QPSX_SPU_OPTIMIZE= \
	QPSX_DISPATCH_CACHE_ENTRIES=64 QPSX_GTE_NATIVE_DIVIDE=1 \
	QPSX_MIPS_PSMEM_REG=1 QPSX_MIPS_PERSISTENT_RETURN_RA=1 \
	QPSX_MIPS_DISPATCH_PREFETCH=0 QPSX_MIPS_DISPATCH_CACHE_GP=1 \
	QPSX_MIPS_DISPATCH_CACHE_GP_TRUST_ABI=0 \
	QPSX_MIPS_DISPATCH_BRANCH_LIKELY=1 \
	QPSX_MIPS_DISPATCH_FRAME_BRANCH_LIKELY=0 \
	QPSX_MIPS_FOLD_DIRECT_JUMPS=1 \
	QPSX_MIPS_FOLD_DIRECT_JUMPS_MAX=8 \
	QPSX_MIPS_FOLD_DIRECT_JUMPS_BYTES=1024 \
	QPSX_GTE_HOT_O3=1 QPSX_GPU_FIXED_FAST_PATH=1 \
	QPSX_GPU_FIXED_LIGHTING=1 QPSX_GPU_LINEAR_4BPP=0 \
	QPSX_GPU_PACKED_TILE_WRITES=1 \
	QPSX_GPU_PACKED_SPRITE_4BPP=1 \
	QPSX_GPU_PACKED_POLY_WRITES=1 \
	QPSX_GPU_4BPP_GOURAUD_FLATV=0 \
	QPSX_GPU_4BPP_GOURAUD_FLATV_MIN_PIXELS=16 \
	QPSX_GPU_4BPP_GOURAUD_CACHE=0 QPSX_GPU_HOT_DRIVER_ORDER=1 \
	QPSX_FASTMEM_HOT_ORDER=1 QPSX_HOT_LAYOUT=0 QPSX_PHASE_METRICS=0 \
	QPSX_GTE_OPCODE_COUNTER=0 QPSX_GTE_INTPL_OPTIMIZE=0 \
	QPSX_GTE_INTPL_COMPACT=0 QPSX_GTE_RTPT_OS=0 \
	QPSX_GTE_RTPT_ASM_FAST=0 QPSX_GPU_4BPP_FLATV=0 \
	QPSX_GPU_4BPP_FLATV_MIN_PIXELS=16 QPSX_GPU_4BPP_FLATV_ROW=0 \
	QPSX_GPU_4BPP_FLATV_ROW_MIN_PIXELS=16 \
	QPSX_GPU_4BPP_PALETTE_LUT=0 QPSX_GPU_4BPP_FULLMASK=0 \
	QPSX_GPU_4BPP_FULLMASK_MIN_PIXELS=16 \
	QPSX_GPU_4BPP_FULLMASK_PACKED_WRITES=0 \
	QPSX_GPU_4BPP_FULLMASK_PACKED_UNROLL=0 QPSX_GPU_DIRECT_PACKET=0 \
	QPSX_PERFORMANCE_FRAME_MARKERS=1 QPSX_FULLMASK_BUILD_SUFFIX= \
	QPSX_FASTMEM_BUILD_SUFFIX= QPSX_LINUX_MIRRORING=0 \
	QPSX_LINUX_RAM_HELPER_FASTPATH=1 QPSX_HLE_LAZY_EVENT_CHECK=0 \
	QPSX_LAYOUT_PAD_BYTES=0 \
	QPSX_MIPS_SCRATCHPAD_ARITH_CLASSIFY=0 \
	QPSX_GE_RAW_VRAM=1 QPSX_GPU_GOURAUD_LINE_FLATFAST=0 \
	QPSX_GPU_POLY_2043_FAST=0 QPSX_GPU_DMA_CHAIN_FAST=0 \
	QPSX_GPU_DMA_CHAIN_ADAPTIVE_MIN_PREV_WORK=0 \
	QPSX_GPU_DMA_CHAIN_ADAPTIVE_DEFER_PREFETCH=0 \
	QPSX_MIPS_FAST_MEM_CONVERT=1 QPSX_MIPS_PROPAGATE_FUZZY_ADDR=0 \
	QPSX_RECMEM_ALIGNMENT=16 QPSX_GPU_RUNTIME_METRICS=0 \
	QPSX_GPU_RECIP_TABLE_BITS=0 QPSX_RUNTIME_TELEMETRY=1 \
	QPSX_PROFILER=0 QPSX_DEV_PROFILER=0 QPSX_DEV_R2=0

QPSX_PROFILE_FRONTIER_PRODUCTION_CONTROL_ARGS := \
	$(QPSX_PROFILE_FRONTIER_COMMON_ARGS) \
	QPSX_PROFILE_ID=frontier-production-control \
	QPSX_BUILD_TAG=qpsx-frontier-prod-asm0 \
	SF2000_FRAME_TAIL_METRICS=0 QPSX_ASM_READS=0 QPSX_MIPS_ASM_MEM_READS=0
# True ASM-read builds execute retro_run but stall Ridge Racer before sustained
# scanout (physical runs 524/525 and the QEMU attract visual gate).  Keep the
# public candidate safe until the helper has a differential ABI/semantic test;
# do not make a black-screen experiment look production-ready by its name.
# Run 529 rejected the 0x640 cache-colour restoration (-0.26% at frame 2700).
# Keep that exact recipe below for reproduction.  The active candidate uses
# an exact unsigned interval classification for generated scratchpad word
# accesses.  It removes one instruction and one dependent branch from this hot
# path; aliases and hardware-register accesses retain the legacy helper path.
QPSX_PROFILE_FRONTIER_PRODUCTION_CANDIDATE_ARGS := \
	$(QPSX_PROFILE_FRONTIER_COMMON_ARGS) \
	QPSX_PROFILE_ID=frontier-production-candidate \
	QPSX_BUILD_TAG=qpsx-frontier-prod-scratcharith \
	SF2000_FRAME_TAIL_METRICS=0 QPSX_MIPS_SCRATCHPAD_ARITH_CLASSIFY=1 \
	QPSX_ASM_READS=0 QPSX_MIPS_ASM_MEM_READS=0
QPSX_PROFILE_FRONTIER_LAYOUT_PAD640_ARGS := \
	$(QPSX_PROFILE_FRONTIER_COMMON_ARGS) \
	QPSX_PROFILE_ID=frontier-layout-pad640 \
	QPSX_BUILD_TAG=qpsx-frontier-prod-pad640 \
	SF2000_FRAME_TAIL_METRICS=0 QPSX_LAYOUT_PAD_BYTES=1600 \
	QPSX_ASM_READS=0 QPSX_MIPS_ASM_MEM_READS=0
QPSX_PROFILE_FRONTIER_TAIL_CONTROL_ARGS := \
	$(QPSX_PROFILE_FRONTIER_COMMON_ARGS) \
	QPSX_PROFILE_ID=frontier-tail-control \
	QPSX_BUILD_TAG=qpsx-frontier-tail-asm0 \
	SF2000_FRAME_TAIL_METRICS=1 QPSX_ASM_READS=0 QPSX_MIPS_ASM_MEM_READS=0
QPSX_PROFILE_FRONTIER_TAIL_CANDIDATE_ARGS := \
	$(QPSX_PROFILE_FRONTIER_COMMON_ARGS) \
	QPSX_PROFILE_ID=frontier-tail-candidate \
	QPSX_BUILD_TAG=qpsx-frontier-tail-safe \
	SF2000_FRAME_TAIL_METRICS=1 QPSX_ASM_READS=0 QPSX_MIPS_ASM_MEM_READS=0

qpsx_profile_args = \
	$(if $(filter frontier-production-control,$(1)),$(QPSX_PROFILE_FRONTIER_PRODUCTION_CONTROL_ARGS),\
	$(if $(filter frontier-production-candidate,$(1)),$(QPSX_PROFILE_FRONTIER_PRODUCTION_CANDIDATE_ARGS),\
	$(if $(filter frontier-layout-pad640,$(1)),$(QPSX_PROFILE_FRONTIER_LAYOUT_PAD640_ARGS),\
	$(if $(filter frontier-tail-control,$(1)),$(QPSX_PROFILE_FRONTIER_TAIL_CONTROL_ARGS),\
	$(if $(filter frontier-tail-candidate,$(1)),$(QPSX_PROFILE_FRONTIER_TAIL_CANDIDATE_ARGS),\
	$(error unknown QPSX profile '$(1)'; choose one of $(QPSX_PROFILE_NAMES)))))))

qpsx_profile_tail = $(if $(findstring -tail-,$(1)),1,0)
qpsx_profile_peer = $(if $(findstring -control,$(1)),$(subst -control,-candidate,$(1)),$(subst -candidate,-control,$(1)))

.PHONY: qpsx-profile-list qpsx-profile-build \
	$(addprefix qpsx-profile-,$(QPSX_PROFILE_NAMES))
# Every profile ultimately links and copies the same qpsx-dev executable.
# The source archive lock is insufficient once that lock is released, so
# parallel named-profile goals must remain sequential through the copy/audit.
.NOTPARALLEL: $(addprefix qpsx-profile-,$(QPSX_PROFILE_NAMES))
qpsx-profile-list:
	@printf '%s\n' $(QPSX_PROFILE_NAMES)

$(addprefix qpsx-profile-,$(QPSX_PROFILE_NAMES)):
	$(MAKE) --no-print-directory qpsx-profile-build \
		QPSX_PROFILE=$(@:qpsx-profile-%=%)

# Keep the manifest recipe outside an eval-generated rule.  Besides being
# easier to audit, this ensures Make does not consume shell-variable dollars
# during two rounds of expansion.
qpsx-profile-build:
	@mkdir -p '$(QPSX_PROFILE_ARTIFACT_ROOT)'
	$(MAKE) --no-print-directory qpsx-dev-mips32r1-audit \
		$(call qpsx_profile_args,$(QPSX_PROFILE))
	@cp '$(QPSX_DEV_EXECUTABLE)' '$(QPSX_PROFILE_ARTIFACT_ROOT)/sf2000-qpsx-$(QPSX_PROFILE)'
	@cp '$(QPSX_DEV_LINK_MAP)' '$(QPSX_PROFILE_ARTIFACT_ROOT)/sf2000-qpsx-$(QPSX_PROFILE).map'
	@set -eu; \
		profile='$(QPSX_PROFILE)'; \
		out='$(QPSX_PROFILE_ARTIFACT_ROOT)'; \
		exe="$$out/sf2000-qpsx-$$profile"; \
		map="$$out/sf2000-qpsx-$$profile.map"; \
		peer_profile='$(call qpsx_profile_peer,$(QPSX_PROFILE))'; \
		peer="$$out/sf2000-qpsx-$$peer_profile"; \
		stamp='$(QPSX_DEV_FLAGS_STAMP)'; \
		test -s "$$exe" && test -s "$$map" && test -s "$$stamp"; \
		if test -f "$$peer" && cmp -s "$$exe" "$$peer"; then \
			echo "QPSX profile $$profile is byte-identical to $$peer_profile" >&2; \
			exit 1; \
		fi; \
		profile_stamp=$$(sed -n 's/^QPSX_PROFILE_ID=//p' "$$stamp" | head -n 1); \
		tail_stamp=$$(sed -n 's/^SF2000_FRAME_TAIL_METRICS=//p' "$$stamp" | head -n 1); \
		scratch_stamp=$$(sed -n 's/^QPSX_MIPS_SCRATCHPAD_ARITH_CLASSIFY=//p' "$$stamp" | head -n 1); \
		fingerprint=$$(sed -n 's/^QPSX_BUILD_FINGERPRINT=//p' "$$stamp" | head -n 1); \
		test "$$profile_stamp" = "$$profile"; \
		test "$$tail_stamp" = '$(call qpsx_profile_tail,$(QPSX_PROFILE))'; \
		if test "$$profile" = frontier-production-candidate; then \
			test "$$scratch_stamp" = 1; \
		else \
			test "$$scratch_stamp" = 0; \
		fi; \
		test -n "$$fingerprint"; \
		if grep -Eq '[[:space:]]psxMemRead(8|16|32)_asm([[:space:]]|$$)' "$$map"; then \
			echo "QPSX safe profile $$profile unexpectedly references quarantined ASM reads" >&2; \
			exit 1; \
		fi; \
		if test "$$profile" = frontier-layout-pad640; then \
			awk '/^[[:space:]]*\.text\.sf2000_qpsx_layout_pad$$/ { getline; if ($$2 == "0x640") ok=1 } END { exit !ok }' "$$map" || { \
				echo "QPSX profile $$profile lacks its 0x640 layout pad" >&2; exit 1; }; \
		fi; \
		frontend_rev=$$(git rev-parse --verify HEAD); \
		frontend_status=$$(git status --porcelain --untracked-files=all); \
		frontend_dirty=0; test -z "$$frontend_status" || frontend_dirty=1; \
		qpsx_rev=$$(git -C '$(QPSX_DEV_SOURCE)' rev-parse --verify HEAD); \
		qpsx_status=$$(git -C '$(QPSX_DEV_SOURCE)' status --porcelain --untracked-files=all); \
		qpsx_dirty=0; test -z "$$qpsx_status" || qpsx_dirty=1; \
		exe_hash=$$(sha256sum "$$exe" | awk '{print $$1}'); \
		map_hash=$$(sha256sum "$$map" | awk '{print $$1}'); \
		manifest="$$out/$$profile.manifest"; \
		tmp="$$manifest.tmp"; \
		{ \
			printf 'profile=%s\n' "$$profile"; \
			printf 'profile_peer=%s\n' "$$peer_profile"; \
			printf 'fingerprint=%s\n' "$$fingerprint"; \
			printf 'effective_key_flags=%s\n' '$(call qpsx_profile_args,$(QPSX_PROFILE))'; \
			printf 'effective_compiler_flags=%s\n' "$$(sed -n 's/^CFLAGS=//p' "$$stamp" | head -n 1)"; \
			printf 'frontend_rev=%s\nfrontend_dirty=%s\n' "$$frontend_rev" "$$frontend_dirty"; \
			printf 'qpsx_rev=%s\nqpsx_dirty=%s\n' "$$qpsx_rev" "$$qpsx_dirty"; \
			printf 'qpsx_patch_id=%s\n' '$(QPSX_PROFILE_PATCH_ID)'; \
			printf 'executable=%s\nexecutable_sha256=%s\n' "$$exe" "$$exe_hash"; \
			printf 'map=%s\nmap_sha256=%s\n' "$$map" "$$map_hash"; \
		} > "$$tmp"; \
		mv "$$tmp" "$$manifest"; \
		cat "$$manifest"
