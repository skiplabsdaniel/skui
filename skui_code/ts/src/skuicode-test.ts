import { launch } from "#skui_code/skui_code";

const chars =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 @#&é\"÷'§è!çà)-=";
const size = chars.length;

function getRandomInt(max: number) {
  return Math.floor(Math.random() * max);
}

function getLine(columns: number) {
  let txtColumns: string[] = [];
  for (let i = 0; i < columns; i++) {
    const v = chars.charAt(getRandomInt(size));
    txtColumns.push(v);
  }
  return txtColumns.join("");
}

function getText(lines: number, columns: number) {
  let txtLines: string[] = [];
  for (let i = 0; i < lines; i++) {
    txtLines.push(getLine(columns));
  }
  return txtLines.join("\n");
}


launch(getText(10000, 80));
