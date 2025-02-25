!/bin/bash
# Pull do git e compilar automaticamente ficheiros de RCOM
# Tem que estar na pasta de git
cd "$(dirname "$0")" || { echo "Erro"; exit 1; }

# Pull do Git
echo "Pulling do Git..."
if git pull; then
    echo "Git pull bom."
else
    echo "Git pull mau."
    exit 1
fi

# Nome dos ficheiros e outputs
SOURCE_FILES=("read_noncanonical.c" "write_noncanonical.c")
OUTPUT_EXECS=("read" "write")


# Compila
for i in "${!SOURCE_FILES[@]}"; do
    src="${SOURCE_FILES[i]}"
    exec="${OUTPUT_EXECS[i]}"
    echo "Compilar $src como $exec..."
    if gcc -Wall -Wextra -o "$exec" "$src"; then
        echo "Compilou bem: $exec"
    else
        echo "Não compilou $src."
        exit 1
    fi
done
