#include "kvm/fdt.h"
#include "kvm/kvm.h"
#include "kvm/kvm-cpu.h"
#include "kvm/util.h"

#include "arm-common/gic.h"
#include "arm-common/spe.h"

#ifdef CONFIG_ARM64
static int set_spe_attr(struct kvm *kvm, int vcpu_idx,
			struct kvm_device_attr *attr)
{
	int ret, fd;

	fd = kvm->cpus[vcpu_idx]->vcpu_fd;

	ret = ioctl(fd, KVM_HAS_DEVICE_ATTR, attr);
	if (!ret) {
		ret = ioctl(fd, KVM_SET_DEVICE_ATTR, attr);
		if (ret)
			pr_err("SPE KVM_SET_DEVICE_ATTR failed (%d)\n", ret);
	} else {
		pr_err("Unsupported SPE on vcpu%d\n", vcpu_idx);
	}

	return ret;
}

void spe__generate_fdt_nodes(void *fdt, struct kvm *kvm)
{
	const char compatible[] = "arm,statistical-profiling-extension-v1";
	int irq = KVM_ARM_SPEV1_PPI;
	int i, ret;

	u32 cpu_mask = (((1 << kvm->nrcpus) - 1) << GIC_FDT_IRQ_PPI_CPU_SHIFT) \
		       & GIC_FDT_IRQ_PPI_CPU_MASK;
	u32 irq_prop[] = {
		cpu_to_fdt32(GIC_FDT_IRQ_TYPE_PPI),
		cpu_to_fdt32(irq - 16),
		cpu_to_fdt32(cpu_mask | IRQ_TYPE_LEVEL_HIGH),
	};

	if (!kvm->cfg.arch.has_spev1)
		return;

	if (!kvm__supports_extension(kvm, KVM_CAP_ARM_SPE_V1)) {
		pr_info("SPE unsupported\n");
		return;
	}

	for (i = 0; i < kvm->nrcpus; i++) {
		struct kvm_device_attr spe_attr;

		spe_attr = (struct kvm_device_attr){
			.group	= KVM_ARM_VCPU_SPE_V1_CTRL,
			.addr	= (u64)(unsigned long)&irq,
			.attr	= KVM_ARM_VCPU_SPE_V1_IRQ,
		};

		ret = set_spe_attr(kvm, i, &spe_attr);
		if (ret)
			return;

		spe_attr = (struct kvm_device_attr){
			.group	= KVM_ARM_VCPU_SPE_V1_CTRL,
			.attr	= KVM_ARM_VCPU_SPE_V1_INIT,
		};

		ret = set_spe_attr(kvm, i, &spe_attr);
		if (ret)
			return;
	}

	_FDT(fdt_begin_node(fdt, "spe"));
	_FDT(fdt_property(fdt, "compatible", compatible, sizeof(compatible)));
	_FDT(fdt_property(fdt, "interrupts", irq_prop, sizeof(irq_prop)));
	_FDT(fdt_end_node(fdt));
}
#else
void spe__generate_fdt_nodes(void *fdt, struct kvm *kvm) { }
#endif
