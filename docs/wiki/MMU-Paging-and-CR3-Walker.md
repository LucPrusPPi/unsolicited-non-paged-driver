# MMU Paging & CR3 Page Table Walker

## Overview

The `Cr3Walker` module provides hardware-level translation of virtual linear addresses into physical addresses by traversing the x86-64 4-level page table hierarchy directly from the CR3 register, bypassing standard Windows API process attachment (`KeStackAttachProcess`).

---

## Linear Address Decomposition (48-bit Canonical)

In 64-bit mode, linear virtual addresses use a 48-bit canonical representation with sign-extension across bits [48..63]:

$$\text{Virtual Address [47:0]} \implies \text{PML4E [47:39]} \to \text{PDPTE [38:30]} \to \text{PDE [29:21]} \to \text{PTE [20:12]} \to \text{Offset [11:0]}$$

| Field | Bit Range | Size | Description |
|---|---|---|---|
| **PML4 Index** | 47:39 | 9 bits | Index into Page Map Level 4 Table (512 entries) |
| **PDPT Index** | 38:30 | 9 bits | Index into Page Directory Pointer Table (512 entries) |
| **PD Index** | 29:21 | 9 bits | Index into Page Directory Table (512 entries) |
| **PT Index** | 20:12 | 9 bits | Index into Page Table (512 entries) |
| **Page Offset** | 11:0 | 12 bits | Physical byte offset within the 4KB page |

---

## Canonical Address Validation (`IsCanonical`)

To prevent CPU general-protection faults (`#GP`), all virtual addresses are validated prior to table dereferencing:

```cpp
constexpr bool IsCanonical(uint64_t va) noexcept {
    const uint64_t signBits = va >> 47;
    return (signBits == 0) || (signBits == 0x1FFFF);
}
```

If bits [48..63] do not match bit 47, the address is non-canonical, and translation immediately returns `0` (or `STATUS_INVALID_PARAMETER`).

---

## 4-Level Translation Math

### 1. PML4 Level
$$\text{PML4E Address} = (\text{CR3} \ \& \ \text{0x000FFFFFFFFFF000}) + (\text{PML4 Index} \times 8)$$
If $\text{PML4E.Present} == 0$, translation aborts.

### 2. PDPT Level
$$\text{PDPTE Address} = (\text{PML4E} \ \& \ \text{0x000FFFFFFFFFF000}) + (\text{PDPT Index} \times 8)$$
If $\text{PDPTE.Present} == 0$, translation aborts.

#### 1GB Huge Page Evaluation:
If $\text{PDPTE.LargePage} == 1$ (Bit 7):
$$\text{Physical Address} = (\text{PDPTE} \ \& \ \text{0x000FFFFFC0000000}) + (\text{VA} \ \& \ \text{0x3FFFFFFF})$$

### 3. Page Directory Level
$$\text{PDE Address} = (\text{PDPTE} \ \& \ \text{0x000FFFFFFFFFF000}) + (\text{PD Index} \times 8)$$
If $\text{PDE.Present} == 0$, translation aborts.

#### 2MB Large Page Evaluation:
If $\text{PDE.LargePage} == 1$ (Bit 7):
$$\text{Physical Address} = (\text{PDE} \ \& \ \text{0x000FFFFFFFE00000}) + (\text{VA} \ \& \ \text{0x1FFFFF})$$

### 4. Page Table Level (Standard 4KB Page)
$$\text{PTE Address} = (\text{PDE} \ \& \ \text{0x000FFFFFFFFFF000}) + (\text{PT Index} \times 8)$$
If $\text{PTE.Present} == 0$, translation aborts.

$$\text{Physical Address} = (\text{PTE} \ \& \ \text{0x000FFFFFFFFFF000}) + (\text{VA} \ \& \ \text{0xFFF})$$

---

## Page-Boundary Clamping (Chunking Algorithm)

When reading or writing contiguous multi-byte buffers across page boundaries, transfers are clamped to remaining bytes within each 4KB physical page to prevent page-fault overruns:

$$\text{chunk} = \min\Big(\text{bytesRemaining}, \ 4096 - (\text{currentVA} \pmod{4096})\Big)$$

This ensures that every physical memory access stays strictly within a single mapped page boundary.
