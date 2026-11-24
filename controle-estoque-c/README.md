# Venda e Controle de Estoque – Manipulação de Arquivos Binários em C

## Integrante

- Tipo de entrega: **Individual**
- Nome: Arthur da Costa Fontenelle
- RA: 00000863847

## Descrição

Programa em C que gerencia vendas e estoque usando **dois arquivos binários**:

| Arquivo         | Registro  | Campos                                                                                                   |
|-----------------|-----------|----------------------------------------------------------------------------------------------------------|
| `produtos.dat`  | `Produto` | id, descrição, preço, quantidade em estoque                                                              |
| `vendas.dat`    | `Venda`   | id, cliente, data, qtd de itens, **vetor `itens[10]` com IDs de produtos**, quantidades, preços, total   |

O campo `itens` da venda referencia o `id` dos registros de `produtos.dat` (relacionamento entre os arquivos).

## Funcionalidades

- **Cadastro** de produtos e registro de vendas (ID gerado automaticamente)
- **Leitura** de um registro específico (por ID) e de todos os registros
- **Atualização** de registros existentes (sobrescrevendo na posição com `fseek`):
  - produto: descrição, preço e estoque
  - venda: cliente e data
- **Controle de estoque automático**: ao registrar uma venda o estoque dos produtos é baixado (com verificação de estoque insuficiente); ao cancelar uma venda os itens voltam ao estoque
- **Remoção** criando um arquivo novo sem o registro excluído, apagando o antigo e renomeando o novo (`temp.dat` → nome original)
- **Relatórios** que mostram a associação entre as entidades:
  1. Cada venda com a **descrição** dos produtos vendidos, quantidades, subtotais e total
  2. Controle de estoque: cada produto com estoque atual, alerta de estoque baixo, total vendido e clientes que o compraram

## Como compilar e executar

```bash
gcc -Wall -Wextra -o estoque main.c
./estoque
```

No Windows (MinGW): `gcc -o estoque.exe main.c` e depois `estoque.exe`.

Os arquivos `produtos.dat` e `vendas.dat` são criados automaticamente na pasta de execução no primeiro cadastro.
